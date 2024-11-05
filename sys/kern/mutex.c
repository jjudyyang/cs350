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
 * thread.  You can access the current thread through curProc[CPU()].
 */

extern Thread *curProc[MAX_CPUS];

void
Mutex_Init(Mutex *mtx, const char *name)
{
    Spinlock_Init(&mtx->lock, name, SPINLOCK_TYPE_NORMAL);
    WaitChannel_Init(&mtx->chan, name);
    return;
}

void
Mutex_Destroy(Mutex *mtx)
{
    WaitChannel_Destroy(&mtx->chan); // destroy wait channel first
    Spinlock_Destroy(&mtx->lock);
    return;
}

/**
 * Mutex_Lock --
 *
 * Acquires the mutex.
 */

void
Mutex_Lock(Mutex *mtx)
{
    /*
     * You cannot hold a spinlock while trying to acquire a Mutex that may 
     * sleep!
     */

    ASSERT(Critical_Level() == 0);

    /* XXXFILLMEIN */

    Spinlock_Lock(&mtx->lock);  // Acquire the mutex's internal spinlock to modify its fields safely

    // //check if the lock is locked 
    // if(mtx->status == MTX_STATUS_LOCKED){
    //     kprintf("the mtx->status is locked");
    // }

    //while mutex is locked, put current thread to sleep
    while(mtx->status == MTX_STATUS_LOCKED){
       // kdebug("The mtx->status is locked\n");
        WaitChannel_Lock(&mtx->chan); // Lock a wait channel before sleeping on it.
        Spinlock_Unlock(&mtx->lock); //release spinlock to avoid deadlock
        //put current thread to sleep until mutex is unlocked
        WaitChannel_Sleep(&mtx->chan);

        // After waking up, re-acquire the spinlock to inspect the mutex again
        Spinlock_Lock(&mtx->lock);
    }

    //now mutex is unlocked so can lock it for this thread by updating the strucut
    mtx->status = MTX_STATUS_LOCKED;
    mtx->owner = Sched_Current(); // set current thread as owner

    //unlock spinlock
    Spinlock_Unlock(&mtx->lock);
    return;
}

/**
 * Mutex_TryLock --
 *
 * Attempts to acquire the user mutex.  Returns EBUSY if the lock is already 
 * taken, otherwise 0 on success.
 */
int
Mutex_TryLock(Mutex *mtx)
{
    /* XXXFILLMEIN */

    //thread that wants to take lock if possible
    //thread does not block if already owned, return fail instead of blocking 

    Spinlock_Lock(&mtx->lock); //accquire lock

    if(mtx->status == MTX_STATUS_LOCKED){
        Spinlock_Unlock(&mtx->lock); //release lock
        return EBUSY;
    }else{
        
        //lock is unlocked!, thread can use 
        mtx->status = MTX_STATUS_LOCKED;
        mtx->owner = Sched_Current(); // set current thread as owner
        Spinlock_Unlock(&mtx->lock); //release lock
        return 0;
    }
}

/**
 * Mutex_Unlock --
 *
 * Releases the user mutex.
 */
void
Mutex_Unlock(Mutex *mtx)
{
    /* XXXFILLMEIN */

     // Acquire the mutex's internal spinlock to modify its fields safely
    Spinlock_Lock(&mtx->lock);

    // Ensure the current thread is the owner of the mutex
    ASSERT(mtx->owner == Sched_Current());
    mtx->status = MTX_STATUS_UNLOCKED;
    mtx->owner = NULL;

    // Wake up a thread waiting on the mutex, if any
    WaitChannel_Wake(&mtx->chan);

    Spinlock_Unlock(&mtx->lock);
    return;
}

