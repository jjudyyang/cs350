
#include <stdbool.h>
#include <stdint.h>

#include <sys/kconfig.h>
#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>
#include <machine/mp.h>
#include <machine/pmap.h>

AS systemAS;
AS *currentAS[MAX_CPUS];

void
PMap_Init()
{
    int i, j;

    kprintf("Initializing PMAP ... ");

    // Setup global state
    for (i = 0; i < MAX_CPUS; i++) {
	currentAS[i] = 0;
    }

    // Allocate system page table
    systemAS.root = PAlloc_AllocPage();
    systemAS.tables = PAGETABLE_ENTRIES / 2 + 1;
    systemAS.mappings = 0;
    if (!systemAS.root)
	PANIC("Cannot allocate system page table");

    for (i = 0; i < PAGETABLE_ENTRIES / 2; i++)
	systemAS.root->entries[i] = 0;

    for (i = PAGETABLE_ENTRIES / 2; i < PAGETABLE_ENTRIES; i++) {
	PageTable *pgtbl = PAlloc_AllocPage();
	PageEntry pte = DMVA2PA((uint64_t)pgtbl) | PTE_W | PTE_P;
	if (!pgtbl)
	    PANIC("Not enough memory!");

	systemAS.root->entries[i] = pte;

	for (j = 0; j < PAGETABLE_ENTRIES; j++) {
	    pgtbl->entries[j] = 0;
	}
    }

    // Setup system mappings
    PMap_SystemLMap(0x0, MEM_DIRECTMAP_BASE + 0x0,
		    3*512, 0); // 3GB RWX
    PMap_SystemLMap(0xC0000000, MEM_DIRECTMAP_BASE + 0xC0000000,
		    512, PTE_NX|PTE_PCD); // 1GB RW + PCD
    PMap_SystemLMap(0x100000000, MEM_DIRECTMAP_BASE + 0x100000000,
		    60*512, 0); // 60GB RWX

    PMap_LoadAS(&systemAS);

    kprintf("Done!\n");
}

void
PMap_InitAP()
{
    PMap_LoadAS(&systemAS);
}

/**
 * PMap_NewAS --
 *
 * Create a new address space.
 *
 * @return Newly created address space.
 */
AS*
PMap_NewAS()
{
    int i;
    AS *as = PAlloc_AllocPage();

    if (!as)
	return NULL;

    as->root = PAlloc_AllocPage();
    as->tables = 1;
    as->mappings = 0;

    if (!as->root) {
	PAlloc_Release(as);
	return NULL;
    }

    for (i = 0; i < PAGETABLE_ENTRIES / 2; i++)
    {
	as->root->entries[i] = 0;
    }
    for (i = PAGETABLE_ENTRIES / 2; i < PAGETABLE_ENTRIES; i++) {
	as->root->entries[i] = systemAS.root->entries[i];
    }

    return as;
}

/**
 * PMap_DestroyAS --
 *
 * Destroys an address space and releases the physical pages.
 *
 * @param [in] space Address space to destroy.
 */
void
PMap_DestroyAS(AS *space)
{
    // Only free the userspace portion (bottom half)
    for (int i = 0; i < PAGETABLE_ENTRIES / 2; i++)
    {
	PageEntry pte = space->root->entries[i];
	if (pte & PTE_P) {
	    // Remove sub-pages
	    PageTable *tbl2 = (PageTable *)DMPA2VA(pte & PGNUMMASK);
	    for (int j = 0; j < PAGETABLE_ENTRIES; j++) {
		PageEntry pte2 = tbl2->entries[j];
		if (pte2 & PTE_P) {
		    PageTable *tbl3 = (PageTable *)DMPA2VA(pte2 & PGNUMMASK);
		    for (int k = 0; k < PAGETABLE_ENTRIES; k++) {
			PageEntry pte3 = tbl3->entries[k];
			if (pte3 & PTE_P) {
			    ASSERT((pte3 & PTE_PS) == 0); // XXX: Large pages not supported
			    PageTable *tbl4 = (PageTable *)DMPA2VA(pte3 & PGNUMMASK);
			    for (int l = 0; l < PAGETABLE_ENTRIES; l++) {
				PageEntry pte4 = tbl4->entries[l];
				if (pte4 & PTE_P) {
				    // Free userspace page
				    PAlloc_Release((void *)DMPA2VA(pte4 & PGNUMMASK));
				}
			    }

			    // Free 3rd level page table page
			    PAlloc_Release((void *)DMPA2VA(pte3 & PGNUMMASK));
			}
		    }

		    // Free 2nd level page table page
		    PAlloc_Release((void *)DMPA2VA(pte2 & PGNUMMASK));
		}
	    }

	    // Free page table page
	    PAlloc_Release((void *)DMPA2VA(pte & PGNUMMASK));
	}
    }

    PAlloc_Release(space);
}

/**
 * PMap_CurrentAS --
 *
 * Get the current address space on this CPU.
 *
 * @return Current address space.
 */
AS *
PMap_CurrentAS()
{
    return currentAS[THISCPU()];
}

/**
 * PMap_LoadAS --
 *
 * Load an address space into the CPU.  Reloads the CR3 register in x86-64 that 
 * points the physical page tables and flushes the TLB entries.
 *
 * @param [in] space Address space to load.
 */
void
PMap_LoadAS(AS *space)
{
    write_cr3(DMVA2PA((uint64_t)space->root));
    currentAS[THISCPU()] = space;
}

/**
 * PMapAllocPageTable --
 *
 * Allocates and initializes a page table.
 *
 * @return Newly created PageTable.
 */
static PageTable *
PMapAllocPageTable()
{
    int i;
    PageTable *pgtbl = PAlloc_AllocPage();

    if (!pgtbl)
	return 0;

    for (i = 0; i < PAGETABLE_ENTRIES; i++) {
	pgtbl->entries[i] = 0;
    }

    return pgtbl;
}

/**
 * PMap_Translate --
 *
 * Translates a virtual address to physical address for a given address space.
 *
 * @param [in] space Address space we wish to lookup a mapping in.
 * @param [in] va Virtual address we wish to translate.
 */
uintptr_t
PMap_Translate(AS *space, uintptr_t va)
{
    int i,j,k,l;
    PageTable *table = space->root;
    PageEntry pte;
    PageEntry *entry;

    i = (va >> (HUGE_PGSHIFT + PGIDXSHIFT)) & PGIDXMASK;
    j = (va >> HUGE_PGSHIFT) & PGIDXMASK;
    k = (va >> LARGE_PGSHIFT) & PGIDXMASK;
    l = (va >> PGSHIFT) & PGIDXMASK;

    pte = table->entries[i];
    if (pte == 0) {
	ASSERT(pte);
	return 0;
    }
    table = (PageTable *)DMPA2VA(pte & PGNUMMASK);

    pte = table->entries[j];
    // XXX: Support 1GB pages
    if (pte == 0) {
	ASSERT(pte);
	return 0;
    }
    table = (PageTable *)DMPA2VA(pte & PGNUMMASK);

    pte = table->entries[k];
    if ((pte & PTE_PS) == PTE_PS) {
	// Handle 2MB pages
	entry = &table->entries[k];
	return (*entry & ~(LARGE_PGMASK | PTE_NX)) + (va & LARGE_PGMASK);
    }
    if (pte == 0) {
	ASSERT(pte);
	return 0;
    }
    table = (PageTable *)DMPA2VA(pte & PGNUMMASK);

    // Handle 4KB pages
    entry = &table->entries[l];

    return (*entry & ~(PGMASK | PTE_NX)) + (va & PGMASK);
}

/**
 * PMapLookupEntry --
 *
 * Lookup a virtual address in a page table and return a pointer to the page 
 * entry.  This function allocates page tables as necessary to fill in the 
 * 4-level heirarchy.
 *
 * @param [in] space Address space to search.
 * @param [in] va Virtual address to lookup.
 * @param [out] entry Pointer will point to the PageEntry.
 * @param [in] size Page size we want to use.
 */
static void
PMapLookupEntry(AS *space, uint64_t va, PageEntry **entry, int size)
{
    int i,j,k,l;
    PageTable *table = space->root;
    PageEntry pte;

    i = (va >> (HUGE_PGSHIFT + PGIDXSHIFT)) & PGIDXMASK;
    j = (va >> HUGE_PGSHIFT) & PGIDXMASK;
    k = (va >> LARGE_PGSHIFT) & PGIDXMASK;
    l = (va >> PGSHIFT) & PGIDXMASK;

    *entry = NULL;

    pte = table->entries[i];
    if (pte == 0) {
	PageTable *newtable = PMapAllocPageTable();
	if (!newtable)
	    return;

	pte = DMVA2PA((uint64_t)newtable) | PTE_P | PTE_W | PTE_U;
	table->entries[i] = pte;
    }
    table = (PageTable *)DMPA2VA(pte & PGNUMMASK);

    pte = table->entries[j];
    if (size == HUGE_PGSIZE) {
	// Handle 1GB pages
	*entry = &table->entries[j];
	return;
    }
    if (pte == 0) {
	PageTable *newtable = PMapAllocPageTable();
	if (!newtable)
	    return;

	pte = DMVA2PA((uint64_t)newtable) | PTE_P | PTE_W | PTE_U;
	table->entries[j] = pte;
    }
    table = (PageTable *)DMPA2VA(pte & PGNUMMASK);

    pte = table->entries[k];
    if (size == LARGE_PGSIZE) {
	// Handle 2MB pages
	*entry = &table->entries[k];
	return;
    }
    if (pte == 0) {
	PageTable *newtable = PMapAllocPageTable();
	if (!newtable)
	    return;

	pte = DMVA2PA((uint64_t)newtable) | PTE_P | PTE_W | PTE_U;
	table->entries[k] = pte;
    }
    table = (PageTable *)DMPA2VA(pte & PGNUMMASK);

    // Handle 4KB pages
    ASSERT(size == PGSIZE);
    *entry = &table->entries[l];
    return;
}

/**
 * PMap_Map --
 *
 * Map a physical to virtual mapping in an address space.
 *
 * @param [in] as Address space.
 * @param [in] phys Physical address.
 * @param [in] virt Virtual address.
 * @param [in] pages Pages to map in.
 * @param [in] flags Flags to apply to the mapping.
 *
 * @retval true On success
 * @retval false On failure
 */
bool
PMap_Map(AS *as, uint64_t phys, uint64_t virt, uint64_t pages, uint64_t flags)
{
    int i;
    PageEntry *entry;

    for (i = 0; i < pages; i++) {
	uint64_t va = virt + PGSIZE * i;
	PMapLookupEntry(as, va, &entry, PGSIZE);
	if (!entry) {
	    kprintf("Map failed to allocate memory!\n");
	    return false;
	}

	*entry = (phys + PGSIZE * i) | PTE_P | PTE_W | PTE_U | flags;
    }

    return true;
}

/**
 * PMap_Unmap --
 *
 * Unmap a range of addresses.
 *
 * @param [in] as Address space.
 * @param [in] va Virtual address.
 * @param [in] pages Pages to map in.
 *
 * @retval true On success
 * @retval false On failure
 */
bool
PMap_Unmap(AS *as, uint64_t va, uint64_t pages)
{
    int i;
    PageEntry *entry;

    for (i = 0; i < pages; i++) {
	uint64_t vai = va + PGSIZE * i;
	PMapLookupEntry(as, vai, &entry, PGSIZE);
	if (!entry) {
	    kprintf("Unmap tried to allocate memory!\n");
	    return false;
	}

	NOT_IMPLEMENTED();

	*entry = 0;
    }

    return true;
}

/**
 * PMap_AllocMap --
 *
 * Map a virtual mapping in an address space and back it by newly allocated 
 * memory.
 *
 * @param [in] as Address space.
 * @param [in] virt Virtual address.
 * @param [in] pages Pages to map in.
 * @param [in] flags Flags to apply to the mapping.
 *
 * @retval true On success
 * @retval false On failure
 */
bool
PMap_AllocMap(AS *as, uint64_t virt, uint64_t len, uint64_t flags)
{
    int i;
    uint64_t pages = (len + PGSIZE - 1) / PGSIZE;
    PageEntry *entry;

    ASSERT((virt & PGMASK) == 0);

    for (i = 0; i < pages; i++) {
	uint64_t va = virt + PGSIZE * i;
	PMapLookupEntry(as, va, &entry, PGSIZE);
	if (!entry) {
	    kprintf("Map failed to allocate memory!\n");
	    return false;
	}

	if ((*entry & PTE_P) != PTE_P) {
	    void *pg = PAlloc_AllocPage();
	    *entry = (uint64_t)DMVA2PA(pg) | PTE_P | PTE_U | flags;
	}
    }

    return true;
}

/**
 * PMap_SystemLookup --
 *
 * Lookup a kernel virtual address in a page table and return a pointer to the 
 * page entry.  This function allocates page tables as necessary to fill in the 
 * 4-level heirarchy.
 *
 * @param [in] va Virtual address to lookup.
 * @param [out] entry Pointer will point to the PageEntry.
 * @param [in] size Page size we want to use.
 */
void
PMap_SystemLookup(uint64_t va, PageEntry **entry, int size)
{
    PMapLookupEntry(&systemAS, va, entry, size);
}

/**
 * PMap_SystemLMap --
 *
 * Map a range of large (2MB) physical pages to virtual pages in the kernel 
 * address space that is shared by all processes.
 *
 * @param [in] phys Physical address.
 * @param [in] virt Virtual address.
 * @param [in] lpages Large pages to map in.
 * @param [in] flags Flags to apply to the mapping.
 *
 * @retval true On success
 * @retval false On failure
 */
bool
PMap_SystemLMap(uint64_t phys, uint64_t virt, uint64_t lpages, uint64_t flags)
{
    int i;
    PageEntry *entry;

    for (i = 0; i < lpages; i++) {
	uint64_t va = virt + LARGE_PGSIZE * i;
	PMapLookupEntry(&systemAS, va, &entry, LARGE_PGSIZE);
	if (!entry) {
	    kprintf("SystemLMap failed to allocate memory!\n");
	    return false;
	}

	*entry = (phys + LARGE_PGSIZE * i) | PTE_P | PTE_W | PTE_PS | flags;
    }

    return true;
}

/**
 * PMap_SystemLMap --
 *
 * Map a range of physical pages to virtual pages in the kernel address space 
 * that is shared by all processes.
 *
 * @param [in] phys Physical address.
 * @param [in] virt Virtual address.
 * @param [in] pages Pages to map in.
 * @param [in] flags Flags to apply to the mapping.
 *
 * @retval true On success
 * @retval false On failure
 */
bool
PMap_SystemMap(uint64_t phys, uint64_t virt, uint64_t pages, uint64_t flags)
{
    int i;
    PageEntry *entry;

    for (i = 0; i < pages; i++) {
	uint64_t va = virt + PGSIZE * i;
	PMapLookupEntry(&systemAS, va, &entry, PGSIZE);
	if (!entry) {
	    kprintf("SystemMap failed to allocate memory!\n");
	    return false;
	}

	*entry = (phys + PGSIZE * i) | PTE_P | PTE_W | flags;
    }

    return true;
}

/**
 * PMap_SystemUnmap --
 *
 * We do not currently use this!
 */
bool
PMap_SystemUnmap(uint64_t virt, uint64_t pages)
{
    NOT_IMPLEMENTED();
    return false;
}

static uint64_t
AddrFromIJKL(uint64_t i, uint64_t j, uint64_t k, uint64_t l)
{
    return (i << 39) | (j << HUGE_PGSHIFT) | (k << LARGE_PGSHIFT) | (l << PGSHIFT);
}

void
PMap_DumpFull(AS *space)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int l = 0;
    PageTable *root = space->root;

    kprintf("Root: %016llx\n", (uint64_t)space->root);

    for (i = 0; i < PAGETABLE_ENTRIES; i++) {
	PageEntry pte = root->entries[i];
	PageTable *l1 = (PageTable *)DMPA2VA(pte & PGNUMMASK);

	if (!(pte & PTE_P))
	    continue;

	kprintf("Level 1: %016llx\n", (uint64_t)pte);

	for (j = 0; j < PAGETABLE_ENTRIES; j++) {
	    PageEntry pte2 = l1->entries[j];
	    PageTable *l2 = (PageTable *)DMPA2VA(pte2 & PGNUMMASK);

	    if (!(pte2 & PTE_P))
		continue;

	    kprintf("Level 2: %016llx\n", (uint64_t)pte2);

	    for (k = 0; k < PAGETABLE_ENTRIES; k++) {
		PageEntry pte3 = l2->entries[k];
		PageTable *l3 = (PageTable *)DMPA2VA(pte3 & PGNUMMASK);

		if (!(pte3 & PTE_P))
		    continue;

		kprintf("Level 3: %016llx:%016llx\n",
			AddrFromIJKL(i, j, k, 0),
			(uint64_t)pte3);

		if ((pte3 & PTE_PS) == 0) {
		    for (l = 0; l < PAGETABLE_ENTRIES; l++) {
			PageEntry pte4 = l3->entries[l];

			kprintf("Level 4: %016llx:%016llx\n",
				AddrFromIJKL(i, j, k, l),
				(uint64_t)pte4);
		    }
		}
	    }
	}
    }

    return;
}

static void
Debug_PMapDumpFull(int argc, const char *argv[])
{
    PMap_DumpFull(currentAS[THISCPU()]);
}

REGISTER_DBGCMD(pmapdumpfull, "Dump memory mappings", Debug_PMapDumpFull);

void
PMap_Dump(AS *space)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int l = 0;
    PageTable *root = space->root;

    kprintf("%-18s  %-18s %-5s\n", "Virtual", "Physical PTE", "Flags");
    for (i = 0; i < PAGETABLE_ENTRIES/2; i++) {
	PageEntry pte = root->entries[i];
	PageTable *l1 = (PageTable *)DMPA2VA(pte & PGNUMMASK);

	if (!(pte & PTE_P))
	    continue;

	for (j = 0; j < PAGETABLE_ENTRIES; j++) {
	    PageEntry pte2 = l1->entries[j];
	    PageTable *l2 = (PageTable *)DMPA2VA(pte2 & PGNUMMASK);

	    if (!(pte2 & PTE_P))
		continue;

	    for (k = 0; k < PAGETABLE_ENTRIES; k++) {
		PageEntry pte3 = l2->entries[k];
		PageTable *l3 = (PageTable *)DMPA2VA(pte3 & PGNUMMASK);

		if (!(pte3 & PTE_P))
		    continue;

		if ((pte3 & PTE_PS) == 0) {
		    for (l = 0; l < PAGETABLE_ENTRIES; l++) {
			PageEntry pte4 = l3->entries[l];

			if (pte4 & PTE_P)
			    kprintf("0x%016llx: 0x%016llx P%c%c%c%c%c\n",
				    AddrFromIJKL(i, j, k, l),
				    (uint64_t)pte4,
				    (pte4 & PTE_W) ? 'W' : ' ',
				    (pte4 & PTE_NX) ? ' ' : 'X',
				    (pte4 & PTE_U) ? 'U' : ' ',
				    (pte4 & PTE_A) ? 'A' : ' ',
				    (pte4 & PTE_D) ? 'D' : ' ');
		    }
		}
	    }
	}
    }

    return;
}

static void
Debug_PMapDump(int argc, const char *argv[])
{
    PMap_Dump(currentAS[THISCPU()]);
}

REGISTER_DBGCMD(pmapdump, "Dump memory mappings", Debug_PMapDump);


