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

// Process List
Spinlock procLock;
uint64_t nextProcessID;
ProcessQueue processList;

// Memory Pools
Slab processSlab;

/*
 * Process
 */

/**
 * Process_Create --
 *
 * Create a process.
 *
 * @param [in] parent Parent process.
 * @param [in] title Process title.
 *
 * @return The newly created process.
 */
Process *
Process_Create(Process *parent, const char *title)
{
    Process *proc = (Process *)Slab_Alloc(&processSlab);

    if (!proc)
	return NULL;

    memset(proc, 0, sizeof(*proc));

    proc->pid = nextProcessID++;
    proc->threads = 0;
    proc->refCount = 1;
    proc->procState = PROC_STATE_NULL;
    TAILQ_INIT(&proc->threadList);

    if (title) {
	strncpy((char *)&proc->title, title, PROCESS_TITLE_LENGTH);
    } else {
	proc->title[0] = '\0';
    }

    proc->space = PMap_NewAS();
    if (proc->space == NULL) {
	Slab_Free(&processSlab, proc);
	return NULL;
    }
    proc->ustackNext = MEM_USERSPACE_STKBASE;

    Spinlock_Init(&proc->lock, "Process Lock", SPINLOCK_TYPE_NORMAL);

    Semaphore_Init(&proc->zombieSemaphore, 0, "Zombie Semaphore");
    TAILQ_INIT(&proc->zombieQueue);

    Handle_Init(proc);

    proc->parent = parent;
    if (parent) {
	Spinlock_Lock(&parent->lock);
	TAILQ_INSERT_TAIL(&parent->childrenList, proc, siblingList);
	Spinlock_Unlock(&parent->lock);
    }
    TAILQ_INIT(&proc->childrenList);
    TAILQ_INIT(&proc->zombieProc);
    Mutex_Init(&proc->zombieProcLock, "Zombie Process Lock");
    CV_Init(&proc->zombieProcCV, "Zombie Process CV");
    CV_Init(&proc->zombieProcPCV, "Zombie Process PCV");

    Spinlock_Lock(&procLock);
    TAILQ_INSERT_TAIL(&processList, proc, processList);
    Spinlock_Unlock(&procLock);

    return proc;
}

/**
 * Process_Destroy --
 *
 * Destroy a process.  This should not be called direct, but rather by 
 * Process_Release.
 *
 * @param [in] proc Process to destroy.
 */
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

    // XXX: We need to promote zombie processes to our parent
    // XXX: Release the semaphore as well

    Spinlock_Lock(&procLock);
    TAILQ_REMOVE(&processList, proc, processList);
    Spinlock_Unlock(&procLock);

    Slab_Free(&processSlab, proc);
}

/**
 * Process_Lookup --
 *
 * Lookup a process by PID and increment its reference count.
 *
 * @param [in] pid Process ID to search for.
 *
 * @retval NULL No such process.
 * @return Process that corresponds to the pid.
 */
Process *
Process_Lookup(uint64_t pid)
{
    Process *p;
    Process *proc = NULL;

    Spinlock_Lock(&procLock);
    TAILQ_FOREACH(p, &processList, processList) {
	if (p->pid == pid) {
	    Process_Retain(p);
	    proc = p;
	    break;
	}
    }
    Spinlock_Unlock(&procLock);

    return proc;
}

/**
 * Process_Retain --
 *
 * Increment the reference count for a given process.
 *
 * @param proc Process to retain a reference to.
 */
void
Process_Retain(Process *proc)
{
    ASSERT(proc->refCount != 0);
    __sync_fetch_and_add(&proc->refCount, 1);
}

/**
 * Process_Release --
 *
 * Decrement the reference count for a given process.
 *
 * @param proc Process to release a reference of.
 */
void
Process_Release(Process *proc)
{
    ASSERT(proc->refCount != 0);
    if (__sync_fetch_and_sub(&proc->refCount, 1) == 1) {
	Process_Destroy(proc);
    }
}

/**
 * Process_Wait --
 *
 * Wait for a process to exit and then cleanup it's references.  If the pid == 
 * 0, we wait for any process, otherwise we wait for a specific process.
 *
 * @param [in] proc Parent process.
 * @param [in] pid Optionally specify the pid of the process to wait on.
 *
 * @retval ENOENT Process ID doesn't exist.
 * @return Exit status of the process that exited or crashed.
 */
uint64_t
Process_Wait(Process *proc, uint64_t pid)
{
    Thread *thr;
    Process *p = NULL;
    uint64_t status;

    // XXXFILLMEIN
    /* 
     * Dummy waitpid implementation that returns an error. Remove and replace
     * with the actual implementation from the assignment description.
     */
    /* XXXREMOVE START */
    return SYSCALL_PACK(ENOSYS, 0);
    /* XXXREMOVE END */

    status = (p->pid << 16) | (p->exitCode & 0xff);

    // Release threads
    Spinlock_Lock(&proc->lock);
    while (!TAILQ_EMPTY(&p->zombieQueue)) {
	thr = TAILQ_FIRST(&p->zombieQueue);
	TAILQ_REMOVE(&p->zombieQueue, thr, schedQueue);
	Spinlock_Unlock(&proc->lock);

	ASSERT(thr->proc->pid != 1);
	Thread_Release(thr);

	Spinlock_Lock(&proc->lock);
    }
    Spinlock_Unlock(&proc->lock);

    // Release process
    Process_Release(p);

    return SYSCALL_PACK(0, status);
}

/*
 * Debugging
 */

void
Process_Dump(Process *proc)
{
    const char *stateStrings[] = {
	"NULL",
	"READY",
	"ZOMBIE"
    };

    kprintf("title      %s\n", proc->title);
    kprintf("pid        %llu\n", proc->pid);
    kprintf("state	%s\n", stateStrings[proc->procState]);
    kprintf("space      %016llx\n", proc->space);
    kprintf("threads    %llu\n", proc->threads);
    kprintf("refCount   %d\n", proc->refCount);
    kprintf("nextFD     %llu\n", proc->nextFD);
}

static void
Debug_Processes(int argc, const char *argv[])
{
    Process *proc;

    /*
     * We don't hold locks in case you the kernel debugger is entered while 
     * holding this lock.
     */
    //Spinlock_Lock(&threadLock);

    TAILQ_FOREACH(proc, &processList, processList)
    {
	kprintf("Process: %d(%016llx)\n", proc->pid, proc);
	Process_Dump(proc);
    }

    //Spinlock_Unlock(&threadLock);
}

REGISTER_DBGCMD(processes, "Display list of processes", Debug_Processes);

static void
Debug_ProcInfo(int argc, const char *argv[])
{
    Thread *thr = curProc[CPU()];

    kprintf("Current Process State:\n");
    Process_Dump(thr->proc);
}

REGISTER_DBGCMD(procinfo, "Display current process state", Debug_ProcInfo);

