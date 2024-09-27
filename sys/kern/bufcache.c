/*
 * Copyright (c) 2006-2022 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/spinlock.h>
#include <sys/disk.h>
#include <sys/bufcache.h>
#include <errno.h>

Spinlock cacheLock;
XMem *diskBuf;

static TAILQ_HEAD(CacheHashTable, BufCacheEntry) *hashTable;
static TAILQ_HEAD(LRUCacheList, BufCacheEntry) lruList;
static uint64_t cacheHit;
static uint64_t cacheMiss;
static uint64_t cacheAlloc;
static Slab cacheEntrySlab;

DEFINE_SLAB(BufCacheEntry, &cacheEntrySlab);

#define CACHESIZE		(16*1024*1024)
#define HASHTABLEENTRIES	128
#define BLOCKSIZE		(16*1024)

/**
 * BufCache_Init --
 *
 * Initialize the system buffer cache.
 */
void
BufCache_Init()
{
    int i;

    Spinlock_Init(&cacheLock, "BufCache Lock", SPINLOCK_TYPE_NORMAL);

    diskBuf = XMem_New();
    if (!diskBuf)
        Panic("BufCache: Cannot create XMem region\n");

    if (!XMem_Allocate(diskBuf, CACHESIZE))
        Panic("BufCache: Cannot back XMem region\n");

    TAILQ_INIT(&lruList);

    hashTable = PAlloc_AllocPage();
    if (!hashTable)
        Panic("BufCache: Cannot allocate hash table\n");
    for (i = 0; i < HASHTABLEENTRIES; i++) {
        TAILQ_INIT(&hashTable[i]);
    }

    Slab_Init(&cacheEntrySlab, "BufCacheEntry Slab", sizeof(BufCacheEntry), 16);

    // Initialize cache
    uintptr_t bufBase = XMem_GetBase(diskBuf);
    for (i = 0; i < CACHESIZE/BLOCKSIZE; i++) {
	BufCacheEntry *e = BufCacheEntry_Alloc();
	if (!e) {
	    Panic("BufCache: Cannot allocate cache entry\n");
	}

	memset(e, 0, sizeof(*e));
	e->disk = NULL;
	e->buffer = (void *)(bufBase + BLOCKSIZE * i);
	TAILQ_INSERT_TAIL(&lruList, e, lruEntry);
    }

    cacheHit = 0;
    cacheMiss = 0;
    cacheAlloc = 0;
}

/**
 * BufCacheLookup --
 *
 * Looks up a buffer cache entry that can be used by BufCache_Alloc or 
 * BufCache_Read to allocate the underlying buffer.
 *
 * @param [in] disk Disk object
 * @param [in] diskOffset Block offset within the disk
 * @param [out] entry If successful, this contains the buffer cache entry.
 *
 * @retval 0 if successful
 * @return ENOENT if not present.
 */
static int
BufCacheLookup(Disk *disk, uint64_t diskOffset, BufCacheEntry **entry)
{
    struct CacheHashTable *table;
    BufCacheEntry *e;

    // Check hash table
    table = &hashTable[diskOffset % HASHTABLEENTRIES];
    TAILQ_FOREACH(e, table, htEntry) {
	if (e->disk == disk && e->diskOffset == diskOffset) {
	    e->refCount++;
	    if (e->refCount == 1) {
		TAILQ_REMOVE(&lruList, e, lruEntry);
	    }
	    *entry = e;
	    return 0;
	}
    }

    *entry = NULL;
    return ENOENT;
}

/**
 * BufCacheAlloc --
 *
 * Allocates a buffer cache entry that can be used by BufCache_Alloc or 
 * BufCache_Read to allocate the underlying buffer..
 *
 * @param [in] disk Disk object
 * @param [in] diskOffset Block offset within the disk
 * @param [out] entry If successful, this contains the buffer cache entry.
 *
 * @retval 0 if successful.
 * @return ENOMEM if there's no buffer cache entries free.
 */
static int
BufCacheAlloc(Disk *disk, uint64_t diskOffset, BufCacheEntry **entry)
{
    struct CacheHashTable *table;
    BufCacheEntry *e;

    // Allocate from LRU list
    e = TAILQ_FIRST(&lruList);
    if (e == NULL) {
	kprintf("BufCache: No space left!\n");
	return ENOMEM;
    }
    TAILQ_REMOVE(&lruList, e, lruEntry);

    // Remove from hash table
    if (e->disk != NULL) {
	table = &hashTable[e->diskOffset % HASHTABLEENTRIES];
	TAILQ_REMOVE(table, e, htEntry);
    }

    // Initialize
    e->disk = disk;
    e->diskOffset = diskOffset;
    e->refCount = 1;

    // Reinsert into hash table
    table = &hashTable[diskOffset % HASHTABLEENTRIES];
    TAILQ_INSERT_HEAD(table, e, htEntry);
    *entry = e;

    return 0;
}

/**
 * BufCache_Alloc --
 *
 * Allocate a buffer cache entry to allow writing new data to disk.
 *
 * @param [in] disk Disk object
 * @param [in] diskOffset Block offset within the disk
 * @param [out] entry If successful, this contains the buffer cache entry.
 *
 * @retval 0 if successful
 * @return Otherwise returns an error code.
 */
int
BufCache_Alloc(Disk *disk, uint64_t diskOffset, BufCacheEntry **entry)
{
    int status;

    Spinlock_Lock(&cacheLock);

    status = BufCacheLookup(disk, diskOffset, entry);
    if (*entry == NULL) {
        status = BufCacheAlloc(disk, diskOffset, entry);
    }

    cacheAlloc++;

    Spinlock_Unlock(&cacheLock);

    return status;
}

/**
 * BufCache_Release --
 *
 * Release a buffer cache entry.  If no other references are held the
 * buffer cache entry is placed on the LRU list.
 *
 * @param [in] entry Buffer cache entry.
 */
void
BufCache_Release(BufCacheEntry *entry)
{
    Spinlock_Lock(&cacheLock);

    entry->refCount--;
    if (entry->refCount == 0) {
        TAILQ_INSERT_TAIL(&lruList, entry, lruEntry);
    }

    Spinlock_Unlock(&cacheLock);
}

/**
 * BufCache_Read --
 *
 * Read block from disk into the buffer cache.
 *
 * @param [in] disk Disk object
 * @param [in] diskOffset Block offset within the disk
 * @param [out] entry If successful, this contains the buffer cache entry.
 * @retval 0 if successful
 * @return Otherwise returns an error code.
 */
int
BufCache_Read(Disk *disk, uint64_t diskOffset, BufCacheEntry **entry)
{
    int status;
    void *buf;
    SGArray sga;

    Spinlock_Lock(&cacheLock);
    status = BufCacheLookup(disk, diskOffset, entry);
    if (*entry != NULL) {
        cacheHit++;
        Spinlock_Unlock(&cacheLock);
        return status;
    }
    cacheMiss++;

    status = BufCacheAlloc(disk, diskOffset, entry);
    if (status != 0) {
        Spinlock_Unlock(&cacheLock);
        return status;
    }

    buf = (*entry)->buffer;
    SGArray_Init(&sga);
    SGArray_Append(&sga, diskOffset, BLOCKSIZE);

    /*
     * XXX: Need to avoid holding cacheLock while reading from the disk, but 
     * need ensure other cores doesn't issue a read to the same block.
     */
    status = Disk_Read(disk, buf, &sga, NULL, NULL);
    Spinlock_Unlock(&cacheLock);

    return status;
}

/**
 * BufCache_Write --
 *
 * Write a buffer cache entry to disk.
 *
 * @retval 0 if successful
 * @return Otherwise an error code is returned.
 */
int
BufCache_Write(BufCacheEntry *entry)
{
    void *buf = entry->buffer;
    SGArray sga;

    SGArray_Init(&sga);
    SGArray_Append(&sga, entry->diskOffset, BLOCKSIZE);

    return Disk_Write(entry->disk, buf, &sga, NULL, NULL);
}

static void
Debug_BufCache(int argc, const char *argv[])
{
    kprintf("Hits: %lld\n", cacheHit);
    kprintf("Misses: %lld\n", cacheMiss);
    kprintf("Allocations: %lld\n", cacheAlloc);
}

REGISTER_DBGCMD(diskcache, "Display disk cache statistics", Debug_BufCache);

