
#ifndef __SYS_KTIMER_H__
#define __SYS_KTIMER_H__

#include <sys/queue.h>

typedef void (*KTimerCB)(void *);

typedef struct KTimerEvent {
    uint64_t			refCount;
    uint64_t			timeout;
    KTimerCB			cb;
    void			*arg;
    LIST_ENTRY(KTimerEvent)	timerQueue;
} KTimerEvent;

KTimerEvent *KTimer_Create(uint64_t timeout, KTimerCB cb, void *arg);
void KTimer_Retain(KTimerEvent *evt);
void KTimer_Release(KTimerEvent *evt);
void KTimer_Cancel(KTimerEvent *evt);
void KTimer_Process();

#endif /* __SYS_KTIMER_H__ */

