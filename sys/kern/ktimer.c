
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/kmem.h>
#include <sys/mp.h>
#include <sys/ktime.h>
#include <sys/ktimer.h>

#define TIMER_WHEEL_LENGTH	256

static int timerHead = 0;
static uint64_t timerNow = 0;
static LIST_HEAD(TimerWheelHead, KTimerEvent) timerSlot[TIMER_WHEEL_LENGTH];
static Spinlock timerLock;
static Slab timerSlab;

DEFINE_SLAB(KTimerEvent, &timerSlab);

void
KTimer_Init()
{
    int i;

    Spinlock_Init(&timerLock, "KTimer Lock", SPINLOCK_TYPE_NORMAL);
    Slab_Init(&timerSlab, "KTimerEvent Slab", sizeof(KTimerEvent), 16);

    // Initialize wheel
    timerHead = 0;
    timerNow = KTime_GetEpoch();
    for (i = 0; i < TIMER_WHEEL_LENGTH; i++) {
	LIST_INIT(&timerSlot[i]);
    }
}

KTimerEvent *
KTimer_Create(uint64_t timeout, KTimerCB cb, void *arg)
{
    int slot;
    KTimerEvent *evt = KTimerEvent_Alloc();

    evt->refCount = 2; // One for the wheel and one for the callee
    evt->timeout = timerNow + timeout;
    evt->cb = cb;
    evt->arg = arg;

    Spinlock_Lock(&timerLock);
    slot = (timerHead + timeout + TIMER_WHEEL_LENGTH - 1) % TIMER_WHEEL_LENGTH;
    // XXX: should insert into tail
    LIST_INSERT_HEAD(&timerSlot[slot], evt, timerQueue);
    Spinlock_Unlock(&timerLock);

    return evt;
}

void
KTimer_Retain(KTimerEvent *evt)
{
    ASSERT(evt->refCount != 0);
    __sync_fetch_and_add(&evt->refCount, 1);
}

void
KTimer_Release(KTimerEvent *evt)
{
    ASSERT(evt->refCount != 0);
    if (__sync_fetch_and_sub(&evt->refCount, 1) == 1) {
	KTimerEvent_Free(evt);
    }
}

void
KTimer_Cancel(KTimerEvent *evt)
{
    Spinlock_Lock(&timerLock);

    LIST_REMOVE(evt, timerQueue);
    KTimer_Release(evt);

    Spinlock_Unlock(&timerLock);
}

void
KTimer_Process()
{
    uint64_t now;

    if (CPU() != 0) {
	return;
    }

    now = KTime_GetEpoch();

    Spinlock_Lock(&timerLock);

    while (now > timerNow) {
	KTimerEvent *it, *tmp;

	// Dispatch pending timer events
	LIST_FOREACH_SAFE(it, &timerSlot[timerHead], timerQueue, tmp) {
	    if (it->timeout <= now) {
		(it->cb)(it->arg);
		LIST_REMOVE(it, timerQueue);
		KTimer_Release(it);
	    }
	}

	// Rotate wheel forward
	timerNow = timerNow + 1;
	timerHead = (timerHead + 1) % TIMER_WHEEL_LENGTH;
    }

    Spinlock_Unlock(&timerLock);
}

