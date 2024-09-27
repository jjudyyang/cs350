/*
 * Copyright (c) 2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/cdefs.h>
#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/queue.h>
#include <sys/thread.h>
#include <sys/spinlock.h>
#include <sys/waitchannel.h>

Spinlock chanListLock;
LIST_HEAD(ChanListHead, WaitChannel) chanList = LIST_HEAD_INITIALIZER(chanList);

void
WaitChannel_EarlyInit()
{
    Spinlock_Init(&chanListLock, "WaitChannel List", SPINLOCK_TYPE_NORMAL);
}

void
WaitChannel_Init(WaitChannel *wchan, const char *name)
{
    TAILQ_INIT(&wchan->chanQueue);
    strncpy(&wchan->name[0], name, WAITCHANNEL_NAMELEN);
    Spinlock_Init(&wchan->lock, name, SPINLOCK_TYPE_NORMAL);

    Spinlock_Lock(&chanListLock);
    LIST_INSERT_HEAD(&chanList, wchan, chanList);
    Spinlock_Unlock(&chanListLock);
}

void
WaitChannel_Destroy(WaitChannel *wchan)
{
    ASSERT(TAILQ_EMPTY(&wchan->chanQueue));

    Spinlock_Lock(&chanListLock);
    LIST_REMOVE(wchan, chanList);
    Spinlock_Unlock(&chanListLock);

    Spinlock_Destroy(&wchan->lock);
}

/**
 * WaitChannel_Lock --
 *
 * Acquires the wait channel lock.
 */
void
WaitChannel_Lock(WaitChannel *wchan)
{
    Spinlock_Lock(&wchan->lock);
}

/**
 * WaitChannel_Sleep --
 *
 * Places the current thread to asleep while releasing the wait channel lock.
 *
 * Side Effect:
 * Retains a reference to thread until the thread is woken up.
 */
void
WaitChannel_Sleep(WaitChannel *wchan)
{
    Thread *thr = Sched_Current();

    Sched_SetWaiting(thr);
    TAILQ_INSERT_TAIL(&wchan->chanQueue, thr, chanQueue);
    Spinlock_Unlock(&wchan->lock);

    Sched_Scheduler();
}

/**
 * WaitChannel_Wake --
 *
 * Wake up a single thread.
 *
 * Side Effects:
 * Releases the thread reference once complete.
 */
void
WaitChannel_Wake(WaitChannel *wchan)
{
    Thread *thr;

    Spinlock_Lock(&wchan->lock);

    thr = TAILQ_FIRST(&wchan->chanQueue);
    if (thr != NULL) {
	TAILQ_REMOVE(&wchan->chanQueue, thr, chanQueue);
	Sched_SetRunnable(thr);
	Thread_Release(thr);
    }

    Spinlock_Unlock(&wchan->lock);
}

/**
 * WaitChannel_WakeAll --
 *
 * Wakes up all threads currently sleeping on the wait channel.
 *
 * Side Effects:
 * Releases all thread references.
 */
void
WaitChannel_WakeAll(WaitChannel *wchan)
{
    Thread *thr;
    Thread *thrTemp;

    Spinlock_Lock(&wchan->lock);

    TAILQ_FOREACH_SAFE(thr, &wchan->chanQueue, chanQueue, thrTemp) {
	TAILQ_REMOVE(&wchan->chanQueue, thr, chanQueue);
	Sched_SetRunnable(thr);
	Thread_Release(thr);
    }

    Spinlock_Unlock(&wchan->lock);
}

