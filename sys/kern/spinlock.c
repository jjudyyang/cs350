/*
 * Copyright (c) 2022-2023 Ali Mashtizadeh
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

#include <machine/atomic.h>
#include <machine/amd64.h>
#include <machine/amd64op.h>

Spinlock lockListLock = {
    0, 0, 0, 0, 0, 0, 0,
    SPINLOCK_TYPE_NORMAL,
    "SPINLOCK LIST",
};
LIST_HEAD(LockListHead, Spinlock) lockList = LIST_HEAD_INITIALIZER(lockList);

TAILQ_HEAD(LockStack, Spinlock) lockStack[MAX_CPUS];

extern uint64_t ticksPerSecond;

void
Spinlock_EarlyInit()
{
    int c;

    for (c = 0; c < MAX_CPUS; c++) {
	TAILQ_INIT(&lockStack[c]);
    }
}

void
Spinlock_Init(Spinlock *lock, const char *name, uint64_t type)
{
    lock->lock = 0;
    lock->cpu = 0;
    lock->count = 0;
    lock->rCount = 0;
    lock->lockTime = 0;
    lock->waitTime = 0;
    lock->type = type;

    strncpy(&lock->name[0], name, SPINLOCK_NAMELEN);

    Spinlock_Lock(&lockListLock);
    LIST_INSERT_HEAD(&lockList, lock, lockList);
    Spinlock_Unlock(&lockListLock);
}

void
Spinlock_Destroy(Spinlock *lock)
{
    Spinlock_Lock(&lockListLock);
    LIST_REMOVE(lock, lockList);
    Spinlock_Unlock(&lockListLock);
}

/**
 * Spinlock_Lock --
 *
 * Spin until we acquire the spinlock.  This will also disable interrupts to 
 * prevent deadlocking with interrupt handlers.
 */
void
Spinlock_Lock(Spinlock *lock) __NO_LOCK_ANALYSIS
{
    uint64_t startTSC;
    Critical_Enter();

    startTSC = Time_GetTSC();
    while (atomic_swap_uint64(&lock->lock, 1) == 1)
    {
	if (lock->type == SPINLOCK_TYPE_RECURSIVE && lock->cpu == CPU()) {
	    break;
	}
	if ((Time_GetTSC() - startTSC) / ticksPerSecond > 1) {
	    kprintf("Spinlock_Lock(%s): waiting for over a second!\n", lock->name);
	    breakpoint();
	}
    }
    lock->waitTime += Time_GetTSC() - startTSC;

    lock->cpu = CPU();
    lock->count++;

    lock->rCount++;
    if (lock->rCount == 1)
	lock->lockedTSC = Time_GetTSC();

    TAILQ_INSERT_TAIL(&lockStack[CPU()], lock, lockStack);
}

/**
 * Spinlock_Unlock --
 *
 * Release the spinlock.  This will re-enable interrupts.
 */
void
Spinlock_Unlock(Spinlock *lock) __NO_LOCK_ANALYSIS
{
    ASSERT(lock->cpu == CPU());

    TAILQ_REMOVE(&lockStack[CPU()], lock, lockStack);

    lock->rCount--;
    if (lock->rCount == 0) {
	lock->cpu = 0;
	lock->lockTime += Time_GetTSC() - lock->lockedTSC;
	atomic_set_uint64(&lock->lock, 0);
    }

    Critical_Exit();
}

bool
Spinlock_IsHeld(Spinlock *lock)
{
    return (lock->cpu == CPU()) && (lock->lock == 1);
}

void
Debug_Spinlocks(int argc, const char *argv[])
{
    Spinlock *lock;

    Spinlock_Lock(&lockListLock);

    kprintf("%-36s Locked CPU    Count     WaitTime     LockTime\n", "Lock Name");
    LIST_FOREACH(lock, &lockList, lockList)
    {
	kprintf("%-36s %6llu %3llu %8llu %12llu %12llu\n", lock->name,
		lock->lock, lock->cpu, lock->count,
		lock->waitTime, lock->lockTime);
    }

    Spinlock_Unlock(&lockListLock);
}

REGISTER_DBGCMD(spinlocks, "Display list of spinlocks", Debug_Spinlocks);

void
Debug_LockStack(int argc, const char *argv[])
{
    int c = CPU();
    Spinlock *lock;

    kprintf("Lock Stack:\n");
    TAILQ_FOREACH(lock, &lockStack[c], lockStack) {
	kprintf("    %s\n", lock->name);
    }
}

REGISTER_DBGCMD(lockstack, "Display stack of held spinlocks", Debug_LockStack);

