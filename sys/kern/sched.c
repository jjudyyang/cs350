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

// Scheduler Queues
/**
 * Scheduler lock that protects all queues.
 */
Spinlock schedLock;
/**
 * All threads currently waiting.
 */
ThreadQueue waitQueue;
/**
 * All runnable threads.
 */
ThreadQueue runnableQueue;
/**
 * Current thread executing on a given CPU.
 */
Thread *curProc[MAX_CPUS];

/*
 * Scheduler Functions
 */

/**
 * Sched_Current() --
 *
 * Get the currently executing thread.  This function retains a reference count 
 * and you must release the reference by calling Thread_Release.
 *
 * @return Returns the current thread.
 */
Thread *
Sched_Current()
{
    Spinlock_Lock(&schedLock);

    Thread *thr = curProc[CPU()];
    Thread_Retain(thr);

    Spinlock_Unlock(&schedLock);

    return thr;
}

/**
 * Sched_SetRunnable --
 *
 * Set the thread to the runnable state and move it from the wait queue if 
 * necessary to the runnable queue.
 *
 * @param [in] thr Thread to be set as runnable.
 */
void
Sched_SetRunnable(Thread *thr)
{
    Spinlock_Lock(&schedLock);

    if (thr->proc->procState == PROC_STATE_NULL)
	thr->proc->procState = PROC_STATE_READY;

    if (thr->schedState == SCHED_STATE_WAITING) {
	thr->waitTime += KTime_GetEpochNS() - thr->waitStart;
	thr->waitStart = 0;
	TAILQ_REMOVE(&waitQueue, thr, schedQueue);
    }
    thr->schedState = SCHED_STATE_RUNNABLE;
    TAILQ_INSERT_TAIL(&runnableQueue, thr, schedQueue);

    Spinlock_Unlock(&schedLock);
}

/**
 * Sched_SetWaiting --
 *
 * Set the thread to the waiting state and move it to the wait queue.  The 
 * thread should be currently running.
 *
 * @param [in] thr Thread to be set as waiting.
 */
void
Sched_SetWaiting(Thread *thr)
{
    Spinlock_Lock(&schedLock);

    ASSERT(thr->schedState == SCHED_STATE_RUNNING);

    thr->schedState = SCHED_STATE_WAITING;
    TAILQ_INSERT_TAIL(&waitQueue, thr, schedQueue);
    thr->waitStart = KTime_GetEpochNS();

    Spinlock_Unlock(&schedLock);
}

/**
 * Sched_SetZombie --
 *
 * Set the thread to the zombie state and move it to the parent processes's 
 * zombie process queue.  The thread should be currently running.
 *
 * @param [in] thr Thread to be set as zombie.
 */
void
Sched_SetZombie(Thread *thr)
{
    Process *proc = thr->proc;

    Spinlock_Lock(&proc->lock);
    uint64_t threads = proc->threads--;
    Spinlock_Unlock(&proc->lock);

    /*
     * Do not go to sleep holding spinlocks.
     */
    ASSERT(Critical_Level() == 0);

    if (threads == 1) {
	// All processes have parents except 'init' and 'kernel'
	ASSERT(proc->parent != NULL);
	Mutex_Lock(&proc->parent->zombieProcLock);
	Spinlock_Lock(&proc->parent->lock); // Guards child list
	proc->procState = PROC_STATE_ZOMBIE;
	TAILQ_REMOVE(&proc->parent->childrenList, proc, siblingList);
	TAILQ_INSERT_TAIL(&proc->parent->zombieProc, proc, siblingList);
	Spinlock_Unlock(&proc->parent->lock);
	CV_Signal(&proc->zombieProcPCV);
	CV_Signal(&proc->parent->zombieProcCV);
    }

    /*
     * Set as zombie just before releasing the zombieProcLock in case we had to 
     * sleep to acquire the zombieProcLock.
     */
    Spinlock_Lock(&schedLock);
    thr->schedState = SCHED_STATE_ZOMBIE;
    Spinlock_Unlock(&schedLock);

    Spinlock_Lock(&proc->lock);
    TAILQ_INSERT_TAIL(&proc->zombieQueue, thr, schedQueue);
    Spinlock_Unlock(&proc->lock);

    if (threads == 1)
	Mutex_Unlock(&proc->parent->zombieProcLock);
}

/**
 * Sched_Switch --
 *
 * Switch between threads.  During the creation of kernel threads (and by proxy 
 * user threads) we may not return through this code path and thus the kernel 
 * thread initialization function must release the scheduler lock.
 *
 * @param [in] oldthr Current thread we are switching from.
 * @param [in] newthr Thread to switch to.
 */
static void
Sched_Switch(Thread *oldthr, Thread *newthr)
{
    // Load AS
    PMap_LoadAS(newthr->space);

    Thread_SwitchArch(oldthr, newthr);
}

/**
 * Sched_Scheduler --
 *
 * Run our round robin scheduler to find the process and switch to it.
 */
void
Sched_Scheduler()
{
    Thread *prev;
    Thread *next;

    Spinlock_Lock(&schedLock);

    // Select next thread
    next = TAILQ_FIRST(&runnableQueue);
    if (!next) {
	/*
	 * There may be no other runnable processes on this core.  This is a 
	 * good opportunity to migrate threads.  We should never hit this case 
	 * once the OS is up and running because of the idle threads, but just 
	 * in case we should assert that we never return to a zombie or waiting 
	 * thread.
	 */
	ASSERT(curProc[CPU()]->schedState == SCHED_STATE_RUNNING);
	Spinlock_Unlock(&schedLock);
	return;
    }
    ASSERT(next->schedState == SCHED_STATE_RUNNABLE);
    TAILQ_REMOVE(&runnableQueue, next, schedQueue);

    prev = curProc[CPU()];
    curProc[CPU()] = next;
    next->schedState = SCHED_STATE_RUNNING;
    next->ctxSwitches++;

    if (prev->schedState == SCHED_STATE_RUNNING) {
	prev->schedState = SCHED_STATE_RUNNABLE;
	TAILQ_INSERT_TAIL(&runnableQueue, prev, schedQueue);
    }

    Sched_Switch(prev, next);

    Spinlock_Unlock(&schedLock);
}

