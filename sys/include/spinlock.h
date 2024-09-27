
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <stdint.h>

#include <sys/cdefs.h>
#include <sys/queue.h>

#define SPINLOCK_NAMELEN    32

#define SPINLOCK_TYPE_NORMAL		1
#define SPINLOCK_TYPE_RECURSIVE		2

typedef struct Spinlock
{
    volatile uint64_t	    lock;
    uint64_t		    cpu;
    uint64_t		    count;
    uint64_t		    rCount;
    uint64_t		    lockTime;
    uint64_t		    waitTime;
    uint64_t		    lockedTSC;
    uint64_t		    type;
    char		    name[SPINLOCK_NAMELEN];
    LIST_ENTRY(Spinlock)    lockList;
    TAILQ_ENTRY(Spinlock)   lockStack;
} __LOCKABLE Spinlock;

void Critical_Init();
void Critical_Enter();
void Critical_Exit();
uint32_t Critical_Level();

void Spinlock_EarlyInit();
void Spinlock_Init(Spinlock *lock, const char *name, uint64_t type);
void Spinlock_Destroy(Spinlock *lock);
void Spinlock_Lock(Spinlock *lock) __LOCK_EX(*lock);
void Spinlock_Unlock(Spinlock *lock) __UNLOCK_EX(*lock);
bool Spinlock_IsHeld(Spinlock *lock) __LOCK_EX_ASSERT(*lock);

#endif /* __SPINLOCK_H__ */

