
#include <stdbool.h>
#include <stdint.h>

#include <sys/kconfig.h>
#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>
#include <machine/pmap.h>
#include <machine/mp.h>

#define MAX_XMEM_REGIONS 1024

typedef struct XMem
{
    bool	inUse;
    uintptr_t	base;
    uintptr_t	maxLength;
    uintptr_t	length;
} XMem;

XMem regions[MAX_XMEM_REGIONS];

void
XMem_Init()
{
    int r;
    uintptr_t regionSize = MEM_XMAP_LEN / MAX_XMEM_REGIONS;

    kprintf("Initializing XMEM ... ");

    for (r = 0; r < MAX_XMEM_REGIONS; r++)
    {
	regions[r].inUse = false;
	regions[r].base = MEM_XMAP_BASE + r * regionSize;
	regions[r].maxLength = regionSize;
	regions[r].length = 0;
    }

    kprintf("Done!\n");
}

XMem *
XMem_New()
{
    int r;

    for (r = 0; r < MAX_XMEM_REGIONS; r++)
    {
	if (!regions[r].inUse) {
	    regions[r].inUse = true;
	    return &regions[r];
	}
    }

    return NULL;
}

void
XMem_Destroy(XMem *xmem)
{
    uintptr_t off;
    PageEntry *entry;

    for (off = 0; off < xmem->length; off += PGSIZE) {
	PMap_SystemLookup(xmem->base + off, &entry, PGSIZE);

	// Compute DM

	// Free Page
    }

    //PMap_SystemUnmap(virt, pages);
}

uintptr_t
XMem_GetBase(XMem *xmem)
{
    return xmem->base;
}

uintptr_t
XMem_GetLength(XMem *xmem)
{
    return xmem->length;
}

bool
XMem_Allocate(XMem *xmem, uintptr_t length)
{
    uint64_t off;

    // We already allocated up to that size
    if (xmem->length > length)
	return true;

    // Allocation too long
    if (length > xmem->maxLength)
	return false;

    for (off = xmem->length; off < length; off += PGSIZE) {
	void *pg = PAlloc_AllocPage();
	if (pg == NULL)
	    return false;

	PMap_SystemMap(DMVA2PA((uint64_t)pg), xmem->base + off, 1, 0);

	xmem->length += PGSIZE;
    }

    return true;
}

static void
Debug_XMemStats(int argc, const char *argv[])
{
    int r;

    kprintf("Region Nr: %16s %16s\n", "Base", "Length");
    for (r = 0; r < MAX_XMEM_REGIONS; r++)
    {
	if (regions[r].inUse) {
	    kprintf("Region %2d: %016llx %016llx\n", r,
		    regions[r].base, regions[r].length);
	}
    }
}

REGISTER_DBGCMD(xmemstats, "XMem statistics", Debug_XMemStats);

