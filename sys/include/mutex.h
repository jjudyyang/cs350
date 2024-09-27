
#ifndef __MUTEX_H__
#define __MUTEX_H__

#define MTX_STATUS_UNLOCKED	0
#define MTX_STATUS_LOCKED	1

typedef struct Mutex {
    uint64_t		status;
    Thread		*owner;
    Spinlock		lock;
    WaitChannel		chan;
    LIST_ENTRY(Mutex)	buckets;
} Mutex;

void Mutex_Init(Mutex *mtx, const char *name);
void Mutex_Destroy(Mutex *mtx);
void Mutex_Lock(Mutex *mtx);
int Mutex_TryLock(Mutex *mtx);
void Mutex_Unlock(Mutex *mtx);

#endif /* __MUTEX_H__ */

