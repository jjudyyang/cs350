/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdint.h>

#include <sys/kassert.h>
#include <sys/sga.h>

void
SGArray_Init(SGArray *sga)
{
    sga->len = 0;
}

int
SGArray_Append(SGArray *sga, uint64_t off, uint64_t len)
{
    ASSERT(sga->len < SGARRAY_MAX_ENTRIES)

    sga->entries[sga->len].offset = off;
    sga->entries[sga->len].length = len;
    sga->len++;

    return sga->len;
}

void
SGArray_Dump(SGArray *sga)
{
    int i;

    kprintf("--- SGArray Begin ---\n");
    for (i = 0; i < sga->len; i++)
    {
	kprintf("%d: %016llx %016llx\n", i, sga->entries[i].offset,
		sga->entries[i].length);
    }
    kprintf("--- SGArray End ---\n");
}

