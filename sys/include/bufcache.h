
#ifndef __SYS_BUFCACHE_H__
#define __SYS_BUFCACHE_H__

#include <sys/queue.h>

typedef struct BufCacheEntry {
    Disk				*disk;
    uint64_t				diskOffset;
    uint64_t				refCount;
    void				*buffer;
    TAILQ_ENTRY(BufCacheEntry)		htEntry;
    TAILQ_ENTRY(BufCacheEntry)		lruEntry;
} BufCacheEntry;

void BufCache_Init();
int BufCache_Alloc(Disk *disk, uint64_t diskOffset, BufCacheEntry **entry);
void BufCache_Release(BufCacheEntry *entry);
int BufCache_Read(Disk *disk, uint64_t diskOffset, BufCacheEntry **entry);
int BufCache_Write(BufCacheEntry *entry);

#endif /* __SYS_BUFCACHE_H__ */

