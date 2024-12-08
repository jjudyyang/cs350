/*
 * Copyright (c) 2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/cdefs.h>
#include <sys/kassert.h>
#include <sys/kconfig.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/mp.h>
#include <sys/queue.h>
#include <sys/thread.h>
#include <sys/spinlock.h>
#include <sys/waitchannel.h>
#include <sys/mutex.h>
#include <errno.h>

/*
 * For debugging so we can assert the owner without holding a reference to the 
 * thread. The current thread can be accessed through curProc[CPU()].
 */
extern Thread *curProc[MAX_CPUS];

/**
 * Mutex_Init --
 * 
 * Initialize the mutex with a spinlock and a wait channel.
 *
 * @param mutex A pointer to the mutex structure to initialize.
 * @param mutexName The name of the mutex for debugging purposes.
 */
void
Mutex_Init(Mutex *mutex, const char *mutexName)
{
    // initialize the spinlock for the mutex
    Spinlock_Init(&mutex->lock, mutexName, SPINLOCK_TYPE_NORMAL);

    // initialize the wait channel for the mutex
    WaitChannel_Init(&mutex->chan, mutexName);

    return;
}

/**
 * Mutex_Destroy --
 * 
 * Destroy the mutex by releasing the associated wait channel and spinlock.
 *
 * @param mutex A pointer to the mutex structure to destroy.
 */
void
Mutex_Destroy(Mutex *mutex)
{
    // destroy the wait channel associated with the mutex
    WaitChannel_Destroy(&mutex->chan);

    // destroy the spinlock associated with the mutex
    Spinlock_Destroy(&mutex->lock);

    return;
}

/**
 * Mutex_Lock --
 * 
 * Acquire the mutex. This function will block and sleep if the mutex is 
 * already held by another thread.
 *
 * @param mutex A pointer to the mutex structure to acquire.
 */
void
Mutex_Lock(Mutex *mutex)
{
    // ensure no spinlock is held while attempting to acquire the mutex
    ASSERT(Critical_Level() == 0);

    // lock the spinlock to access mutex properties safely
    Spinlock_Lock(&mutex->lock);

    // if the mutex is already locked, put the thread to sleep
    while (mutex->status == 1) {
        // lock the wait channel to safely sleep
        WaitChannel_Lock(&mutex->chan);

        // unlock the spinlock before sleeping to allow other threads access
        Spinlock_Unlock(&mutex->lock);

        // put the thread to sleep until the mutex becomes available
        WaitChannel_Sleep(&mutex->chan);

        // re-lock the spinlock upon waking up
        Spinlock_Lock(&mutex->lock);
    }

    // mark the mutex as locked and set the current thread as the owner
    mutex->status = 1;
    mutex->owner = curProc[CPU()];

    // unlock the spinlock as the mutex is now acquired
    Spinlock_Unlock(&mutex->lock);
}

/**
 * Mutex_TryLock --
 * 
 * Attempt to acquire the mutex without blocking. If the mutex is already 
 * locked, return EBUSY; otherwise, lock the mutex and return 0.
 *
 * @param mutex A pointer to the mutex structure to attempt to acquire.
 *
 * @return 0 if the mutex was successfully acquired, EBUSY if already locked.
 */
int
Mutex_TryLock(Mutex *mutex)
{
    // if the mutex is not locked, acquire it and set the current thread as owner
    if (mutex->status == 0) {
        mutex->status = 1;
        mutex->owner = curProc[CPU()];
        Spinlock_Unlock(&mutex->lock);
        return 0; // success
    } else {
        // if the mutex is already locked, return EBUSY
        Spinlock_Unlock(&mutex->lock);
        return EBUSY;
    }
}

/**
 * Mutex_Unlock --
 * 
 * Release the mutex and wake up any threads waiting on it.
 *
 * @param mutex A pointer to the mutex structure to release.
 */
void
Mutex_Unlock(Mutex *mutex)
{
    // lock the spinlock to safely modify mutex properties
    Spinlock_Lock(&mutex->lock);

    // mark the mutex as unlocked and clear the owner reference
    mutex->status = 0;
    mutex->owner = NULL;

    // wake up any threads waiting on the mutex
    WaitChannel_Wake(&mutex->chan);

    // unlock the spinlock as the mutex has been successfully released
    Spinlock_Unlock(&mutex->lock);

    return;
}

//reuploaded
