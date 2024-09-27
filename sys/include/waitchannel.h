
#ifndef __WAITCHANNEL_H__
#define __WAITCHANNEL_H__

#include <sys/cdefs.h>
#include <sys/queue.h>

#define WAITCHANNEL_NAMELEN    32

typedef struct WaitChannel
{
    Spinlock				lock;
    char				name[WAITCHANNEL_NAMELEN];
    LIST_ENTRY(WaitChannel)		chanList;
    TAILQ_HEAD(WaitQueue, Thread)	chanQueue;
} WaitChannel;

void WaitChannel_EarlyInit();
void WaitChannel_Init(WaitChannel *wc, const char *name);
void WaitChannel_Destroy(WaitChannel *wc);
void WaitChannel_Lock(WaitChannel *wc) __LOCK_EX(wc->lock);
void WaitChannel_Sleep(WaitChannel *wc) __UNLOCK_EX(wc->lock);
void WaitChannel_Wake(WaitChannel *wc);
void WaitChannel_WakeAll(WaitChannel *wc);

#endif /* __WAITCHANNEL_H__ */

