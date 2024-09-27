
#include <stdbool.h>
#include <stdint.h>

#include <syscall.h>
#include <core/mutex.h>

void
CoreMutex_Init(CoreMutex *mtx)
{
    mtx->lock = 0;
}


void
CoreMutex_Lock(CoreMutex *mtx)
{
    while (__sync_lock_test_and_set(&mtx->lock, 1) == 1) {
	OSThreadSleep(0);
    }
}

bool
CoreMutex_TryLock(CoreMutex *mtx)
{
    if (__sync_lock_test_and_set(&mtx->lock, 1) == 1) {
	return false;
    } else {
	return true;
    }
}

void
CoreMutex_Unlock(CoreMutex *mtx)
{
    __sync_lock_release(&mtx->lock);
}

