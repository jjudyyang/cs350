/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include <sys/cdefs.h>
#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/queue.h>
#include <sys/spinlock.h>

// PGSIZE
#include <machine/amd64.h>
#include <machine/pmap.h>

/* 'FREEPAGE' */
#define FREEPAGE_MAGIC_FREE	0x4652454550414745ULL
/* 'ALLOCATE' */
#define FREEPAGE_MAGIC_INUSE	0x414c4c4f43415445ULL

Spinlock pallocLock;
uint64_t totalPages;
uint64_t freePages;

typedef struct FreePage
{
    uint64_t			magic;
    LIST_ENTRY(FreePage)	entries;
} FreePage;

typedef struct PageInfo
{
    uint64_t			refCount;
} PageInfo;

XMem *pageInfoXMem;
PageInfo *pageInfoTable;
uint64_t pageInfoLength;
LIST_HEAD(FreeListHead, FreePage) freeList;

/*
 * Initializes the page allocator
 */
void
PAlloc_Init()
{
    totalPages = 0;
    freePages = 0;

    Spinlock_Init(&pallocLock, "PAlloc Lock", SPINLOCK_TYPE_NORMAL);

    LIST_INIT(&freeList);
    pageInfoXMem = NULL;
    pageInfoTable = NULL;
}

/**
 * PAlloc_LateInit --
 *
 * The late init call is made after the page tables are initialized using a 
 * small boot memory region (2nd 16MBs).  This is where initialize the XMem 
 * region that represents the PageInfo array, and map memory into it.
 */
void
PAlloc_LateInit()
{
    void *pageInfoOld = pageInfoTable;

    pageInfoXMem = XMem_New();
    if (!XMem_Allocate(pageInfoXMem, pageInfoLength)) {
	Panic("Cannot back pageInfoTable!");
    }

    pageInfoTable = (PageInfo *)XMem_GetBase(pageInfoXMem);
    memcpy(pageInfoTable, pageInfoOld, pageInfoLength);

    // Free old pages
}

/**
 * PAlloc_AddRegion --
 *
 * Add a physical memory region to the page allocator.
 */
void
PAlloc_AddRegion(uintptr_t start, uintptr_t len)
{
    uintptr_t i;
    FreePage *pg;

    if ((start % PGSIZE) != 0)
	Panic("Region start is not page aligned!");
    if ((len % PGSIZE) != 0)
	Panic("Region length is not page aligned!");

    /*
     * PageInfo table isn't initialized on the first call to this function.  We 
     * must allocate a temporary table that will be copied into the XMem region 
     * inside PAlloc_LateInit.
     *
     * Note that the PageInfo table is invalid for regions that are not added 
     * to the free list such as MMIO regions.
     */
    if (pageInfoTable == NULL) {
	// Physical Address Offsets
	uintptr_t base = (uintptr_t)DMVA2PA(start);
	uintptr_t end = base + len;

	pageInfoLength = ROUNDUP(end / PGSIZE * sizeof(PageInfo), PGSIZE);
	pageInfoTable = (PageInfo *)start;

	start += pageInfoLength;
	len -= pageInfoLength;

	for (i = 0; i < (base / PGSIZE); i++) {
	    pageInfoTable[i].refCount = 1;
	}
	for (i = (base / PGSIZE); i < (end / PGSIZE); i++) {
	    pageInfoTable[i].refCount = 0;
	}
	for (i = 0; i < (pageInfoLength / PGSIZE); i++) {
	    pageInfoTable[i + (base / PGSIZE)].refCount = 1;
	}
    } else {
	/*
	 * Only the first call to AddRegion should occur before the XMem region 
	 * is initialized.
	 */
	
	ASSERT(pageInfoXMem != NULL);

	uintptr_t base = (uintptr_t)DMVA2PA(start);
	uintptr_t end = base + len;

	uintptr_t newLength = ROUNDUP(end / PGSIZE * sizeof(PageInfo), PGSIZE);

	if (!XMem_Allocate(pageInfoXMem, newLength))
	    Panic("Cannot allocate XMem region!");

	// Initialize new pages
	for (i = (base / PGSIZE); i < (end / PGSIZE); i++) {
	    pageInfoTable[i].refCount = 0;
	}
    }

    Spinlock_Lock(&pallocLock);
    for (i = 0; i < len; i += PGSIZE)
    {
	pg = (void *)(start + i);
	pg->magic = FREEPAGE_MAGIC_FREE;

	totalPages++;
	freePages++;

	LIST_INSERT_HEAD(&freeList, pg, entries);
    }
    Spinlock_Unlock(&pallocLock);
}

/**
 * PAllocGetInfo --
 *
 * Lookup the PageInfo structure for a given physical address.
 */
static inline PageInfo *
PAllocGetInfo(void *pg)
{
    uintptr_t entry = (uintptr_t)DMVA2PA(pg) / PGSIZE;
    return &pageInfoTable[entry];
}

/**
 * PAlloc_AllocPage --
 *
 * Allocate a physical page and return the page's address in the Kernel's ident 
 * mapped memory region.
 *
 * @retval NULL if no memory is available.
 * @return Newly allocated physical page.
 */
void *
PAlloc_AllocPage()
{
    PageInfo *info;
    FreePage *pg;

    Spinlock_Lock(&pallocLock);
    pg = LIST_FIRST(&freeList);
    ASSERT(pg != NULL);
    LIST_REMOVE(pg, entries);

    ASSERT(pg->magic == FREEPAGE_MAGIC_FREE);

    info = PAllocGetInfo(pg);
    ASSERT(info != NULL);
    ASSERT(info->refCount == 0);
    info->refCount++;

    pg->magic = FREEPAGE_MAGIC_INUSE;

    freePages--;
    Spinlock_Unlock(&pallocLock);

    memset(pg, 0, PGSIZE);

    return (void *)pg;
}

/**
 * PAllocFreePage --
 *
 * Free a page.
 */
static void
PAllocFreePage(void *region)
{
    FreePage *pg = (FreePage *)region;

    ASSERT(((uintptr_t)region % PGSIZE) == 0);

    LIST_INSERT_HEAD(&freeList, pg, entries);

#ifndef NDEBUG
    // Application can write this magic, but for
    // debug builds we can use this as a double free check.
    ASSERT(pg->magic != FREEPAGE_MAGIC_FREE);

    PageInfo *info = PAllocGetInfo(pg);
    ASSERT(info->refCount == 0);
#endif

    pg->magic = FREEPAGE_MAGIC_FREE;
    freePages++;
}

/**
 * PAlloc_Retain --
 *
 * Increment the reference count for a physical page.
 */
void
PAlloc_Retain(void *pg)
{
    PageInfo *info = PAllocGetInfo(pg);

    Spinlock_Lock(&pallocLock);
    ASSERT(info->refCount != 0);
    info->refCount++;
    Spinlock_Unlock(&pallocLock);
}

/**
 * PAlloc_Release --
 *
 * Deccrement the reference count for a physical page.  If the reference count 
 * is zero the page will be freed.
 */
void
PAlloc_Release(void *pg)
{
    PageInfo *info = PAllocGetInfo(pg);

    Spinlock_Lock(&pallocLock);
    ASSERT(info->refCount != 0);
    info->refCount--;
    if (info->refCount == 0)
	PAllocFreePage(pg);
    Spinlock_Unlock(&pallocLock);
}

static void
Debug_PAllocStats(int argc, const char *argv[])
{
    kprintf("Total Pages: %llu\n", totalPages);
    kprintf("Allocated Pages: %llu\n", totalPages - freePages);
    kprintf("Free Pages: %llu\n", freePages);
}

REGISTER_DBGCMD(pallocstats, "Page allocator statistics", Debug_PAllocStats);

static void
Debug_PAllocDump(int argc, const char *argv[])
{
    struct FreePage *it;

    LIST_FOREACH(it, &freeList, entries) {
	if (it->magic != FREEPAGE_MAGIC_FREE)
	    kprintf("Magic Corrupted! (%lx)\n", it->magic);
	kprintf("Free %lx\n", (uintptr_t)it);
    }
}

REGISTER_DBGCMD(pallocdump, "Dump page allocator's free list", Debug_PAllocDump);

