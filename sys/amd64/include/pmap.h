
#ifndef __PMAP_H__
#define __PMAP_H__

#include <machine/amd64.h>

/*
 * +----------------------+
 * | Zero Page (Unmapped) |
 * +----------------------+ 0x00000000_00000100 User Space Start
 * |                      |
 * | User Space           |
 *            .
 *            .
 *            .
 * |                      |
 * +----------------------+ 0x00008000_00000000 User Space End
 * |                      |
 * | Non-canonical        |
 *            .
 *            .
 *            .
 * |                      |
 * +----------------------+ 0xFFFF8000_00000000 Direct Map
 * | Direct Map           |
 * +----------------------+ 0xFFFF8100_00000000 XMap Start
 * | XMap                 |
 * +----------------------+ 0xFFFF8120_00000000 XMap Top
 *
 *
 */
#define MEM_USERSPACE_BASE	0x0000000000000000ULL
#define MEM_USERSPACE_LEN	0x0000800000000000ULL
#define MEM_USERSPACE_TOP	(MEM_USERSPACE_BASE + MEM_USERSPACE_LEN)

#define MEM_USERSPACE_STKBASE	0x0000000070000000ULL
#define MEM_USERSPACE_STKLEN	0x0000000000010000ULL
#define MEM_USERSPACE_STKTOP	(MEM_USERSPACE_STKBASE + MEM_USERSPACE_STKLEN)

#define MEM_DIRECTMAP_BASE	0xFFFF800000000000ULL
#define MEM_DIRECTMAP_LEN	0x0000010000000000ULL
#define MEM_XMAP_BASE		0xFFFF810000000000ULL
#define MEM_XMAP_LEN		0x0000002000000000ULL

#define PPN2DMVA(ppn)		(((ppn) << PGSIZE) + MEM_DIRECTMAP_BASE)
#define DMVA2PPN(dmva)		(((dmva) - MEM_DIRECTMAP_BASE) >> PGSIZE)
#define DMVA2PA(dmva)		((dmva) - MEM_DIRECTMAP_BASE)
#define DMPA2VA(pa)		((pa) + MEM_DIRECTMAP_BASE)
#define VA2PA(va)		PMap_Translate(PMap_CurrentAS(), va)

typedef struct AS
{
    PageTable	*root;
    uint64_t	tables;
    uint64_t	mappings;
} AS;

void PMap_Init();
void PMap_InitAP();

AS* PMap_NewAS();
void PMap_DestroyAS(AS *space);
AS* PMap_CurrentAS();
void PMap_LoadAS(AS *space);
void PMap_Dump(AS *space);

uintptr_t PMap_Translate(AS *space, uintptr_t va);

// Manipulate User Memory
bool PMap_Map(AS *as, uint64_t phys, uint64_t virt, uint64_t pages, uint64_t flags);
bool PMap_AllocMap(AS *as, uint64_t virt, uint64_t len, uint64_t flags);
bool PMap_Unmap(AS *as, uint64_t virt, uint64_t pages);

// Manipulate Kernel Memory
void PMap_SystemLookup(uint64_t va, PageEntry **entry, int size);
bool PMap_SystemLMap(uint64_t phys, uint64_t virt, uint64_t lpages, uint64_t flags);
bool PMap_SystemMap(uint64_t phys, uint64_t virt, uint64_t pages, uint64_t flags);
bool PMap_SystemUnmap(uint64_t virt, uint64_t pages);

#endif /* __PMAP_H__ */

