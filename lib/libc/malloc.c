
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

typedef struct Header {
    uint16_t		magic;
    uint16_t		status;
    uint32_t		size;
    struct Header	*next;
} Header;

#define HEAP_MAGIC	0x8051

#define HEAP_MIN_POOLSIZE		(64 - sizeof(Header))
#define HEAP_MAX_POOLSIZE		(128*1024 - sizeof(Header))
#define HEAP_POOLS			(12)

#define PGSIZE		4096
#define HEAP_INCREMENT	(PGSIZE / 64)

typedef struct HeapPool {
    uint32_t		blocksInuse;
    uint32_t		blocksFree;
    Header		*free;
    uint64_t		base;
    uint64_t		top;
} HeapPool;

#define ROUNDUP(_x, _n) (((_x) + (_n) - 1) & ~((_n) - 1))

static HeapPool pool[HEAP_POOLS] = {
    { 0, 0, (Header *)0, 0x400000000, 0x400000000 }, // 64B
    { 0, 0, (Header *)0, 0x410000000, 0x410000000 }, // 128B
    { 0, 0, (Header *)0, 0x420000000, 0x420000000 }, // 256B
    { 0, 0, (Header *)0, 0x430000000, 0x430000000 }, // 512B
    { 0, 0, (Header *)0, 0x440000000, 0x440000000 }, // 1KB
    { 0, 0, (Header *)0, 0x450000000, 0x450000000 }, // 2KB
    { 0, 0, (Header *)0, 0x460000000, 0x460000000 }, // 4KB
    { 0, 0, (Header *)0, 0x470000000, 0x470000000 }, // 8KB
    { 0, 0, (Header *)0, 0x480000000, 0x480000000 }, // 16KB
    { 0, 0, (Header *)0, 0x490000000, 0x490000000 }, // 32KB
    { 0, 0, (Header *)0, 0x4A0000000, 0x4A0000000 }, // 64KB
    { 0, 0, (Header *)0, 0x4B0000000, 0x4B0000000 }, // 128KB
};

static HeapPool largePool = { 0, 0, (Header *)0, 0x4C000000, 0x4C000000 }; // 128KB+

static int
size_to_idx(size_t sz)
{
    int i;
    size_t bucketSz = 64;

    for (i = 0; ; i++) {
	if ((bucketSz - sizeof(Header)) >= sz) {
	    return i;
	}

	bucketSz *= 2;
    }
}

static void
grow_small(int idx)
{
    int i;
    size_t bucketSz = (1 << idx) * 64; // Compute bucket size
    char *addr = (char *)pool[idx].top;

    addr = (char *)mmap(addr, bucketSz * HEAP_INCREMENT,
			PROT_READ|PROT_WRITE, MAP_ANON|MAP_FIXED,
			-1, 0);
    if (addr == NULL) {
	return;
    }

    // Update top pointer
    pool[idx].top += bucketSz * HEAP_INCREMENT;

    for (i = 0; i < HEAP_INCREMENT; i++) {
	Header *obj = (Header *)(addr + i * bucketSz);

	obj->magic = HEAP_MAGIC;
	obj->status = 0;
	obj->size = bucketSz - sizeof(Header);
	obj->next = pool[idx].free;
	pool[idx].free = obj;
    }
}

static void *
malloc_small(size_t sz)
{
    int idx = size_to_idx(sz);
    Header *hdr;

    // Grow pool if freelist is empty
    if (pool[idx].free == 0)
	grow_small(idx);

    // Malloc failed
    if (pool[idx].free == 0)
	return NULL;

    hdr = pool[idx].free;
    pool[idx].free = hdr->next;

    return (void *)(hdr + 1);
}

static void
free_small(Header *mem)
{
    int idx = size_to_idx(mem->size);

    mem->next = pool[idx].free;
    pool[idx].free = mem;
}

static void *
malloc_large(size_t sz)
{
    uintptr_t ptr = largePool.top;
    uintptr_t realSz = ROUNDUP(sz, 4096);
    Header *addr;

    addr = (Header *)mmap((void *)ptr, realSz,
			  PROT_READ|PROT_WRITE, MAP_ANON|MAP_FIXED,
			  -1, 0);
    if (addr == NULL) {
	return NULL;
    }

    addr->magic = HEAP_MAGIC;
    addr->status = 0;
    addr->size = realSz;
    addr->next = 0;

    largePool.top += realSz;

    return (void *)(addr + 1);
}

static void
free_large(Header *mem)
{
    munmap(mem, mem->size);
}

void *
calloc(size_t num, size_t sz)
{
    return malloc(num*sz);
}

void *
malloc(size_t sz)
{
    if (sz > HEAP_MAX_POOLSIZE)
	return malloc_large(sz);
    else
	return malloc_small(sz);
}

void
free(void *mem)
{
    Header *hdr = (Header *)mem;
    hdr--;

    if (mem == NULL)
	return;

    assert(hdr->magic == HEAP_MAGIC);

    if (hdr->size > HEAP_MAX_POOLSIZE)
	free_large(hdr);
    else
	free_small(hdr);
}

