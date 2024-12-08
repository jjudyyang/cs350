/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/kassert.h>
#include <sys/kconfig.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/ktime.h>
#include <sys/mp.h>
#include <sys/spinlock.h>
#include <sys/thread.h>
#include <machine/trap.h>
#include <machine/pmap.h>

extern Thread *curProc[MAX_CPUS];

Spinlock procLock;
uint64_t nextProcessID;
ProcessQueue processList;
Slab processSlab;

Process *
Process_Create(Process *parent, const char *title)
{
    Process *newProc = (Process *)Slab_Alloc(&processSlab);
    if (!newProc)
        return NULL;

    memset(newProc, 0, sizeof(*newProc));

    newProc->pid = nextProcessID++;
    newProc->refCount = 1;
    newProc->procState = PROC_STATE_NULL;
    newProc->threads = 0;
    TAILQ_INIT(&newProc->threadList);

    if (title) {
        strncpy(newProc->title, title, PROCESS_TITLE_LENGTH - 1);
        newProc->title[PROCESS_TITLE_LENGTH - 1] = '\0';
    } else {
        newProc->title[0] = '\0';
    }

    newProc->space = PMap_NewAS();
    if (!newProc->space) {
        Slab_Free(&processSlab, newProc);
        return NULL;
    }
    newProc->ustackNext = MEM_USERSPACE_STKBASE;

    Spinlock_Init(&newProc->lock, "Process Lock", SPINLOCK_TYPE_NORMAL);
    Semaphore_Init(&newProc->zombieSemaphore, 0, "Zombie Semaphore");
    TAILQ_INIT(&newProc->zombieQueue);

    Handle_Init(newProc);

    newProc->parent = parent;
    if (parent) {
        Spinlock_Lock(&parent->lock);
        TAILQ_INSERT_TAIL(&parent->childrenList, newProc, siblingList);
        Spinlock_Unlock(&parent->lock);
    }
    TAILQ_INIT(&newProc->childrenList);
    TAILQ_INIT(&newProc->zombieProc);
    Mutex_Init(&newProc->zombieProcLock, "Zombie Process Lock");
    CV_Init(&newProc->zombieProcCV, "Zombie Process CV");
    CV_Init(&newProc->zombieProcPCV, "Zombie Process PCV");

    Spinlock_Lock(&procLock);
    TAILQ_INSERT_TAIL(&processList, newProc, processList);
    Spinlock_Unlock(&procLock);

    return newProc;
}

static void
Process_Destroy(Process *proc)
{
    Handle_Destroy(proc);

    Spinlock_Destroy(&proc->lock);
    Semaphore_Destroy(&proc->zombieSemaphore);
    CV_Destroy(&proc->zombieProcPCV);
    CV_Destroy(&proc->zombieProcCV);
    Mutex_Destroy(&proc->zombieProcLock);
    PMap_DestroyAS(proc->space);

    Spinlock_Lock(&procLock);
    TAILQ_REMOVE(&processList, proc, processList);
    Spinlock_Unlock(&procLock);

    Slab_Free(&processSlab, proc);
}

Process *
Process_Lookup(uint64_t pid)
{
    Process *current;
    Process *result = NULL;

    Spinlock_Lock(&procLock);
    TAILQ_FOREACH(current, &processList, processList) {
        if (current->pid == pid) {
            Process_Retain(current);
            result = current;
            break;
        }
    }
    Spinlock_Unlock(&procLock);

    return result;
}

void
Process_Retain(Process *proc)
{
    ASSERT(proc->refCount > 0);
    __sync_fetch_and_add(&proc->refCount, 1);
}

void
Process_Release(Process *proc)
{
    ASSERT(proc->refCount > 0);
    if (__sync_fetch_and_sub(&proc->refCount, 1) == 1) {
        Process_Destroy(proc);
    }
}

uint64_t
Process_Wait(Process *parent, uint64_t pid)
{
    Thread *thr;
    Process *target = NULL;
    uint64_t exitStatus;

    Mutex_Lock(&parent->zombieProcLock);
    if (pid == 0) {
        while (TAILQ_EMPTY(&parent->zombieProc)) {
            CV_Wait(&parent->zombieProcCV, &parent->zombieProcLock);
        }
        target = TAILQ_FIRST(&parent->zombieProc);
        TAILQ_REMOVE(&parent->zombieProc, target, siblingList);
    } else {
        target = Process_Lookup(pid);
        while (target && target->procState != PROC_STATE_ZOMBIE) {
            CV_Wait(&target->zombieProcPCV, &parent->zombieProcLock);
        }
        if (target) {
            TAILQ_REMOVE(&parent->zombieProc, target, siblingList);
            Process_Release(target);
        }
    }
    Mutex_Unlock(&parent->zombieProcLock);

    if (!target)
        return SYSCALL_PACK(ENOENT, 0);

    exitStatus = (target->pid << 16) | (target->exitCode & 0xff);

    Spinlock_Lock(&parent->lock);
    while (!TAILQ_EMPTY(&target->zombieQueue)) {
        thr = TAILQ_FIRST(&target->zombieQueue);
        TAILQ_REMOVE(&target->zombieQueue, thr, schedQueue);
        Spinlock_Unlock(&parent->lock);

        ASSERT(thr->proc->pid != 1);
        Thread_Release(thr);

        Spinlock_Lock(&parent->lock);
    }
    Spinlock_Unlock(&parent->lock);

    Process_Release(target);

    return SYSCALL_PACK(0, exitStatus);
}

void
Process_Dump(Process *proc)
{
    const char *states[] = {
        "NULL",
        "READY",
        "ZOMBIE"
    };

    kprintf("title      %s\n", proc->title);
    kprintf("pid        %llu\n", proc->pid);
    kprintf("state      %s\n", states[proc->procState]);
    kprintf("space      %016llx\n", proc->space);
    kprintf("threads    %llu\n", proc->threads);
    kprintf("refCount   %d\n", proc->refCount);
    kprintf("nextFD     %llu\n", proc->nextFD);
}

static void
Debug_Processes(int argc, const char *argv[])
{
    Process *p;

    TAILQ_FOREACH(p, &processList, processList) {
        kprintf("Process: %d(%016llx)\n", p->pid, p);
        Process_Dump(p);
    }
}

static void
Debug_ProcInfo(int argc, const char *argv[])
{
    Thread *current = curProc[CPU()];

    kprintf("Current Process State:\n");
    Process_Dump(current->proc);
}

REGISTER_DBGCMD(processes, "Display list of processes", Debug_Processes);
REGISTER_DBGCMD(procinfo, "Display current process state", Debug_ProcInfo);
