
#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include <sys/queue.h>
#include <sys/spinlock.h>

#define SEMAPHORE_NAMELEN   32

struct Thread;

typedef struct Semaphore
{
    Spinlock					lock;
    char					name[SEMAPHORE_NAMELEN];
    int						count;
    TAILQ_HEAD(SemaThreadQueue,Thread)		waiters;
    LIST_ENTRY(Semaphore)			semaphoreList;
} Semaphore;

void Semaphore_Init(Semaphore *sema, int count, const char *name);
void Semaphore_Destroy(Semaphore *sema);
void Semaphore_Acquire(Semaphore *sema);
// bool TimedAcquire(Semaphore *sema, uint64_t timeout);
void Semaphore_Release(Semaphore *sema);
bool Semaphore_TryAcquire(Semaphore *sema);

#endif /* __SEMAPHORE_H__ */

