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

/*
 * Unfortunately the thread, process and scheduler code are pretty well 
 * integrated.  To avoid poluting the global namespace we import a few symbols 
 * from sched.c and process.c that are required during initialization and 
 * regular execution of the thread code.
 */

/* Globals declared in sched.c */
extern Spinlock schedLock;
extern ThreadQueue waitQueue;
extern ThreadQueue runnableQueue;
extern Thread *curProc[MAX_CPUS];

/* Globals declared in process.c */
extern Spinlock procLock;
extern uint64_t nextProcessID;
extern ProcessQueue processList;
extern Slab processSlab;

// Special Kernel Process
Process *kernelProcess;

// Memory Pools
Slab threadSlab;

void Handle_GlobalInit();

void
Thread_Init()
{
    nextProcessID = 1;

    Slab_Init(&processSlab, "Process Objects", sizeof(Process), 16);
    Slab_Init(&threadSlab, "Thread Objects", sizeof(Thread), 16);

    Spinlock_Init(&procLock, "Process List Lock", SPINLOCK_TYPE_NORMAL);
    Spinlock_Init(&schedLock, "Scheduler Lock", SPINLOCK_TYPE_RECURSIVE);

    TAILQ_INIT(&waitQueue);
    TAILQ_INIT(&runnableQueue);
    TAILQ_INIT(&processList);

    Handle_GlobalInit();

    // Kernel Process
    kernelProcess = Process_Create(NULL, "kernel");

    // Create an thread object for current context
    Process *proc = Process_Create(NULL, "init");
    curProc[0] = Thread_Create(proc);
    curProc[0]->schedState = SCHED_STATE_RUNNING;
}

void
Thread_InitAP()
{
    Thread *apthr = Thread_Create(kernelProcess);

    apthr->schedState = SCHED_STATE_RUNNING;

    //PAlloc_Release((void *)thr->kstack);
    //thr->kstack = 0;

    curProc[CPU()] = apthr;
}

/*
 * Thread
 */

Thread *
Thread_Create(Process *proc)
{
    Thread *thr = (Thread *)Slab_Alloc(&threadSlab);

    if (!thr)
	return NULL;

    memset(thr, 0, sizeof(*thr));

    ASSERT(proc != NULL);

    thr->tid = proc->nextThreadID++;
    thr->kstack = (uintptr_t)PAlloc_AllocPage();
    if (thr->kstack == 0) {
	Slab_Free(&threadSlab, thr);
	return NULL;
    }

    Process_Retain(proc);

    Spinlock_Lock(&proc->lock);
    thr->proc = proc;
    proc->threads++;
    TAILQ_INSERT_TAIL(&proc->threadList, thr, threadList);
    thr->space = proc->space;
    thr->ustack = proc->ustackNext;
    proc->ustackNext += MEM_USERSPACE_STKLEN;
    Spinlock_Unlock(&proc->lock);

    thr->schedState = SCHED_STATE_NULL;
    thr->timerEvt = NULL;
    thr->refCount = 1;

    Thread_InitArch(thr);
    // Initialize queue

    return thr;
}

Thread *
Thread_KThreadCreate(void (*f)(void *), void *arg)
{
    Thread *thr = Thread_Create(kernelProcess);
    if (!thr)
	return NULL;

    Thread_SetupKThread(thr, f, (uintptr_t)arg, 0, 0);

    return thr;
}

Thread *
Thread_UThreadCreate(Thread *oldThr, uint64_t rip, uint64_t arg)
{
    Process *proc = oldThr->proc;
    Thread *thr = (Thread *)Slab_Alloc(&threadSlab);

    if (!thr)
	return NULL;

    memset(thr, 0, sizeof(*thr));

    thr->tid = proc->nextThreadID++;
    thr->kstack = (uintptr_t)PAlloc_AllocPage();
    if (thr->kstack == 0) {
	Slab_Free(&threadSlab, thr);
	return NULL;
    }

    thr->space = oldThr->space;
    thr->schedState = SCHED_STATE_NULL;
    thr->refCount = 1;

    Spinlock_Lock(&proc->lock);
    thr->ustack = proc->ustackNext;
    proc->ustackNext += MEM_USERSPACE_STKLEN;
    Spinlock_Unlock(&proc->lock);

    PMap_AllocMap(thr->space, thr->ustack, MEM_USERSPACE_STKLEN, PTE_W);
    // XXX: Check failure

    Thread_InitArch(thr);
    // Initialize queue

    Thread_SetupUThread(thr, rip, arg);

    Process_Retain(proc);

    Spinlock_Lock(&proc->lock);
    thr->proc = proc;
    // XXX: Process lock
    proc->threads++;
    TAILQ_INSERT_TAIL(&proc->threadList, thr, threadList);
    Spinlock_Unlock(&proc->lock);

    return thr;
}

static void
Thread_Destroy(Thread *thr)
{
    Process *proc = thr->proc;

    // Don't free kernel threads
    ASSERT(proc->pid != 1);

    // Free userspace stack

    Spinlock_Lock(&proc->lock);
    TAILQ_REMOVE(&proc->threadList, thr, threadList);
    Spinlock_Unlock(&proc->lock);

    // Free AS
    PAlloc_Release((void *)thr->kstack);

    // Release process handle
    Process_Release(thr->proc);

    Slab_Free(&threadSlab, thr);
}

/**
 * Thread_Lookup --
 *
 * Lookup a thread by TID and increment its reference count.
 *
 * @param [in] proc Process within which to find a specific thread.
 * @param [in] tid Thread ID of the thread to find.
 *
 * @retval NULL if the thread isn't found.
 */
Thread *
Thread_Lookup(Process *proc, uint64_t tid)
{
    Thread *t;
    Thread *thr = NULL;

    Spinlock_Lock(&proc->lock);
    TAILQ_FOREACH(t, &proc->threadList, threadList) {
	if (t->tid == tid) {
	    Thread_Retain(t);
	    thr = t;
	    break;
	}
    }
    Spinlock_Unlock(&proc->lock);

    return thr;
}

/**
 * Thread_Retain --
 *
 * Increment the reference count for a given thread.
 */
void
Thread_Retain(Thread *thr)
{
    ASSERT(thr->refCount != 0);
    __sync_fetch_and_add(&thr->refCount, 1);
}

/**
 * Thread_Release --
 *
 * Decrement the reference count for a given thread.
 */
void
Thread_Release(Thread *thr)
{
    ASSERT(thr->refCount != 0);
    if (__sync_fetch_and_sub(&thr->refCount, 1) == 1) {
	Thread_Destroy(thr);
    }
}

/**
 * Thread_Wait --
 *
 * Wait for any thread (tid == TID_ANY) or a specific thread.
 */
uint64_t
Thread_Wait(Thread *thr, uint64_t tid)
{
    Thread *t;
    uint64_t status;

    ASSERT(thr->proc != NULL);

    if (tid == TID_ANY) {
	t = TAILQ_FIRST(&thr->proc->zombieQueue);
	if (!t) {
	    return SYSCALL_PACK(EAGAIN, 0);
	}

	TAILQ_REMOVE(&thr->proc->zombieQueue, t, schedQueue);
	status = t->exitValue;
	Thread_Release(t);
	return SYSCALL_PACK(0, status);
    }

    // XXXURGENT
    TAILQ_FOREACH(t, &thr->proc->zombieQueue, schedQueue) {
	if (t->tid == tid) {
	    TAILQ_REMOVE(&thr->proc->zombieQueue, t, schedQueue);
	    status = t->exitValue;
	    Thread_Release(t);
	    return SYSCALL_PACK(0, status);
	}
    }

    return 0;
}

extern TaskStateSegment64 TSS[MAX_CPUS];

void
ThreadKThreadEntry(TrapFrame *tf) __NO_LOCK_ANALYSIS
{
    TSS[CPU()].rsp0 = curProc[CPU()]->kstack + 4096;

    Spinlock_Unlock(&schedLock);

    Trap_Pop(tf);
}

/*
 * Debugging
 */

void
Thread_Dump(Thread *thr)
{
    const char *states[] = {
	"NULL",
	"RUNNABLE",
	"RUNNING",
	"WAITING",
	"ZOMBIE"
    };

    // Thread_DumpArch(thr)
    kprintf("space      %016llx\n", thr->space);
    kprintf("kstack     %016llx\n", thr->kstack);
    kprintf("tid        %llu\n", thr->tid);
    kprintf("refCount   %d\n", thr->refCount);
    kprintf("state      %s\n", states[thr->schedState]);
    kprintf("ctxswtch   %llu\n", thr->ctxSwitches);
    kprintf("utime      %llu\n", thr->userTime);
    kprintf("ktime      %llu\n", thr->kernTime);
    kprintf("wtime      %llu\n", thr->waitTime);
    if (thr->proc) {
	Process_Dump(thr->proc);
    }
}

static void
Debug_Threads(int argc, const char *argv[])
{
    Thread *thr;

    //Spinlock_Lock(&threadLock);

    for (int i = 0; i < MAX_CPUS; i++) {
	thr = curProc[i];
	if (thr) {
	    kprintf("Running Thread CPU %d: %d(%016llx) %d\n", i, thr->tid, thr, thr->ctxSwitches);
	    Thread_Dump(thr);
	}
    }
    TAILQ_FOREACH(thr, &runnableQueue, schedQueue)
    {
	kprintf("Runnable Thread: %d(%016llx) %d\n", thr->tid, thr, thr->ctxSwitches);
	Thread_Dump(thr);
    }
    TAILQ_FOREACH(thr, &waitQueue, schedQueue)
    {
	kprintf("Waiting Thread: %d(%016llx) %d\n", thr->tid, thr, thr->ctxSwitches);
	Thread_Dump(thr);
    }

    //Spinlock_Unlock(&threadLock);
}

REGISTER_DBGCMD(threads, "Display list of threads", Debug_Threads);

static void
Debug_ThreadInfo(int argc, const char *argv[])
{
    Thread *thr = curProc[CPU()];

    kprintf("Current Thread State:\n");
    Thread_Dump(thr);
}

REGISTER_DBGCMD(threadinfo, "Display current thread state", Debug_ThreadInfo);

