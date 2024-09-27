/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/kconfig.h>
#include <sys/kdebug.h>
#include <sys/ktime.h>
#include <sys/mp.h>
#include <sys/spinlock.h>
#include <sys/semaphore.h>
#include <sys/thread.h>

Spinlock semaListLock = { 0, 0, 0, 0, 0, 0, 0, 0, "Semaphore List" };
LIST_HEAD(SemaListHead, Semaphore) semaList = LIST_HEAD_INITIALIZER(semaList);

extern uint64_t ticksPerSecond;

void
Semaphore_Init(Semaphore *sema, int count, const char *name)
{
    Spinlock_Init(&sema->lock, name, SPINLOCK_TYPE_NORMAL);
    sema->count = count;

    strncpy(&sema->name[0], name, SEMAPHORE_NAMELEN);
    TAILQ_INIT(&sema->waiters);

    Spinlock_Lock(&semaListLock);
    LIST_INSERT_HEAD(&semaList, sema, semaphoreList);
    Spinlock_Unlock(&semaListLock);
}

void
Semaphore_Destroy(Semaphore *sema)
{
    Spinlock_Lock(&semaListLock);
    LIST_REMOVE(sema, semaphoreList);
    Spinlock_Unlock(&semaListLock);

    Spinlock_Destroy(&sema->lock);
}

void
Semaphore_Acquire(Semaphore *sema)
{
    Thread *cur = Sched_Current();

    while (1) {
	Spinlock_Lock(&sema->lock);
	if (sema->count > 0) {
	    sema->count -= 1;
	    Spinlock_Unlock(&sema->lock);
	    Thread_Release(cur);
	    return;
	}

	// Add to sleeper list
	TAILQ_INSERT_TAIL(&sema->waiters, cur, semaQueue);
	Sched_SetWaiting(cur);

	Spinlock_Unlock(&sema->lock);
	Sched_Scheduler();
    }
}

void
Semaphore_Release(Semaphore *sema)
{
    Thread *thr;

    Spinlock_Lock(&sema->lock);
    sema->count += 1;

    // Wakeup thread
    thr = TAILQ_FIRST(&sema->waiters);
    if (thr != NULL) {
	TAILQ_REMOVE(&sema->waiters, thr, semaQueue);
	Sched_SetRunnable(thr);
    }
    Spinlock_Unlock(&sema->lock);
}

bool
Semaphore_TryAcquire(Semaphore *sema)
{
    Spinlock_Lock(&sema->lock);
    if (sema->count > 0) {
	sema->count -= 1;
	Spinlock_Unlock(&sema->lock);
	return true;
    }
    Spinlock_Unlock(&sema->lock);
    return false;
}

void
Debug_Semaphores(int argc, const char *argv[])
{
    Semaphore *sema;

    Spinlock_Lock(&semaListLock);

    kprintf("%-36s   Count\n", "Lock Name");
    LIST_FOREACH(sema, &semaList, semaphoreList)
    {
	Thread *thr;
	kprintf("%-36s %8d\n", sema->name, sema->count);
	TAILQ_FOREACH(thr, &sema->waiters, semaQueue) {
	    kprintf("waiting: %d:%d\n", thr->proc->pid, thr->tid);
	}
    }

    Spinlock_Unlock(&semaListLock);
}

REGISTER_DBGCMD(semaphores, "Display list of semaphores", Debug_Semaphores);

