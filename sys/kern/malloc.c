/*
 * Copyright (c) 2013-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdarg.h>
#include <stdint.h>

#include <kassert.h>

typedef struct TLSFBlock
{
    struct TLSFBlock	*prevBlock;
    uint64_t		size;
    // Only valid for free blocks
    struct TLSFBlock	*prev;
    struct TLSFBlock	*next;
} TLSFBlock;

typedef struct Heap
{
    uint64_t		magic;

    // Lock

    // Debug statistics
    uint64_t		poolSize;
    uint64_t		poolAllocs;

    // Free list
    uint32_t		flVector;
    uint32_t		slVector[FL_SIZE];
    struct TLSFBlock	*blocks[SL_SIZE][FL_SIZE];
} Heap;

Heap*
Malloc_Create()
{
}

void
Malloc_Destroy(Heap *heap)
{
}

void*
Malloc_Alloc(Heap *heap, uint64_t len)
{
}

void
Malloc_Free(Heap *heap, void *buf)
{
}

bool
Malloc_Realloc(Heap *heap, void *buf, uint64_t newlen)
{
}

