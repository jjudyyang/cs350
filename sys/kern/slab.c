/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/cdefs.h>
#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/queue.h>
#include <sys/kmem.h>

#include <machine/pmap.h>

LIST_HEAD(SlabListHead, Slab) slabList = LIST_HEAD_INITIALIZER(slabList);

/**
 * Slab_Init --
 *
 *	Create's a slab for a single type of object in the kernel.
 *
 *	@param [in] slab Slab that the object belongs to.
 *	@param [in] name Developer friendly name for debugging purposes.
 *	@param [in] objsz Size of the object in bytes.
 *	@param [in] align Alignment of the object in bytes.
 */
void
Slab_Init(Slab *slab, const char *name, uintptr_t objsz, uintptr_t align)
{
    ASSERT(objsz >= sizeof(SlabElement));

    slab->objsz = objsz;
    slab->align = align;
    slab->xmem = XMem_New();
    slab->objs = 0;
    slab->freeObjs = 0;
    slab->allocs = 0;
    slab->frees = 0;
    LIST_INIT(&slab->freeList);

    ASSERT(slab->xmem != NULL);

    strncpy(&slab->name[0], name, SLAB_NAMELEN);

    Spinlock_Init(&slab->lock, name, SPINLOCK_TYPE_NORMAL);

    LIST_INSERT_HEAD(&slabList, slab, slabList);
}

/**
 * SlabExtend --
 *
 *	Grow the slab to allocate new objects.
 *
 *	@param [in] slab Slab that we want to expand.
 *	@retval -1 Failed to expand the slab.
 *	@retval 0 Success.
 */
int
SlabExtend(Slab *slab)
{
    uintptr_t base = XMem_GetBase(slab->xmem);
    uintptr_t len = XMem_GetLength(slab->xmem);
    uintptr_t inc;
    uintptr_t realObjSz = ROUNDUP(slab->objsz, slab->align);

    inc = ROUNDUP(realObjSz * 64, PGSIZE);
    if (inc < 4 * PGSIZE) {
	inc = 4 * PGSIZE;
    }

    if (!XMem_Allocate(slab->xmem, len + inc)) {
	kprintf("Slab: Cannot grow XMem region!\n");
	return -1;
    }

    // Add empty objects to linked list
    uintptr_t i;
    uintptr_t objs = inc / realObjSz;
    for (i = 0; i < objs; i++) {
	SlabElement *elem = (SlabElement *)(base + len + i * realObjSz);

	LIST_INSERT_HEAD(&slab->freeList, elem, free);
    }

    slab->objs += objs;
    slab->freeObjs += objs;

    return 0;
}

/**
 * Slab_Alloc --
 *
 *	Free a slab object.
 *
 *	@param [in] slab Slab that the object belongs to.
 *	@retval NULL Could not allocate an object.
 *	@return Pointer to the allocated object.
 */
void *
Slab_Alloc(Slab *slab)
{
    SlabElement *elem;

    Spinlock_Lock(&slab->lock);

    if (slab->freeObjs == 0)
	SlabExtend(slab);

    elem = LIST_FIRST(&slab->freeList);
    if (elem != NULL) {
	LIST_REMOVE(elem, free);
	slab->allocs++;
	slab->freeObjs--;
    }

    Spinlock_Unlock(&slab->lock);

    return (void *)elem;
}

/**
 * Slab_Free --
 *
 *	Free a slab object.
 *
 *	@param [in] slab Slab that the object belongs to.
 *	@param [in] region Object to free.
 */
void
Slab_Free(Slab *slab, void *region)
{
    Spinlock_Lock(&slab->lock);

    SlabElement *elem = (SlabElement *)region;
    LIST_INSERT_HEAD(&slab->freeList, elem, free);
    slab->frees++;
    slab->freeObjs++;

    Spinlock_Unlock(&slab->lock);
}

static void
Debug_Slabs(int argc, const char *argv[])
{
    Slab *slab;

    kprintf("%-36s %-10s %-10s %-10s\n", "Slab Name", "Alloc", "Free", "Total");
    LIST_FOREACH(slab, &slabList, slabList) {
	kprintf("%-36s %-10lld %-10lld %-10lld\n", slab->name,
		slab->objs - slab->freeObjs, slab->freeObjs, slab->objs);
    }
}

REGISTER_DBGCMD(slabs, "Display list of slabs", Debug_Slabs);

