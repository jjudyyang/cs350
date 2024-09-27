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
#include <sys/cv.h>
#include <errno.h>

void
CV_Init(CV *cv, const char *name)
{
    WaitChannel_Init(&cv->chan, name);

    return;
}

void
CV_Destroy(CV *cv)
{
    WaitChannel_Destroy(&cv->chan);

    return;
}

/**
 * CV_Wait --
 *
 * Wait to be woken up on a condition.
 */
void
CV_Wait(CV *cv, Mutex *mtx)
{
    /* Do not go to sleep holding a spinlock! */
    ASSERT(Critical_Level() == 0);
    /* XXXFILLMEIN */
}

/**
 * CV_Signal --
 *
 * Wake a single thread waiting on the condition.
 */
void
CV_Signal(CV *cv)
{
    /* XXXFILLMEIN */
}

/**
 * CV_Broadcast --
 *
 * Wake all threads waiting on the condition.
 */
void
CV_Broadcast(CV *cv)
{
    /* XXXFILLMEIN */
}

