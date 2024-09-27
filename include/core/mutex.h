
#ifndef __CORE_MUTEX_H__
#define __CORE_MUTEX_H__

typedef struct CoreMutex {
    uint64_t	lock;
} CoreMutex;

void CoreMutex_Init(CoreMutex *mtx);
void CoreMutex_Lock(CoreMutex *mtx);
bool CoreMutex_TryLock(CoreMutex *mtx);
void CoreMutex_Unlock(CoreMutex *mtx);

#endif /* __CORE_MUTEX_H__ */

