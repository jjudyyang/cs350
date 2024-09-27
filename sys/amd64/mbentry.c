/*
 * Multiboot C Entry
 */

#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/cdefs.h>

#include "../dev/console.h"

#include <machine/amd64.h>
#include <machine/pmap.h>
#include <machine/multiboot.h>

void MachineBoot_Entry(unsigned long magic, unsigned long addr);

#define CHECK_FLAG(flag, bit) ((flag) & (1 << (bit)))

#define PAGE_ALIGN __attribute__((aligned(PGSIZE)))
#define DATA_SECTION __attribute__((section(".data")))

extern void Machine_EarlyInit();
extern void Machine_Init();
extern void PAlloc_AddRegion(uintptr_t start, uintptr_t len);

PAGE_ALIGN DATA_SECTION PageTable bootpgtbl3 = { .entries = {
    0x0000000000000183,
    0x0000000000200183,
    0x0000000000400183,
    0x0000000000600183,
    0x0000000000800183,
    0x0000000000A00183,
    0x0000000000C00183,
    0x0000000000E00183,
    0x0000000001000183,
    0x0000000001200183,
    0x0000000001400183,
    0x0000000001600183,
    0x0000000001800183,
    0x0000000001A00183,
    0x0000000001C00183,
    0x0000000001E00183,
    0x0000000002000183,
}};

PAGE_ALIGN DATA_SECTION PageTable bootpgtbl2 = { .entries = {
    ((uint64_t)&bootpgtbl3 - MEM_DIRECTMAP_BASE) + 0x03,
    0
}};

PAGE_ALIGN DATA_SECTION PageTable bootpgtbl1 = { .entries = {
    ((uint64_t)&bootpgtbl2 - MEM_DIRECTMAP_BASE) + 0x03,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 16
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 32
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 48
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 64
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 80
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 96
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 112
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 128
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 144
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 160
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 176
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 192
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 208
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 224
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 240
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 256
    [256] = ((uint64_t)&bootpgtbl2 - MEM_DIRECTMAP_BASE) + 0x03,
}};

#define MAX_REGIONS 16

static uintptr_t memRegionStart[MAX_REGIONS];
static uintptr_t memRegionLen[MAX_REGIONS];
static int memRegionIdx;

void
MachineBoot_Entry(unsigned long magic, unsigned long addr)
{
    multiboot_info_t *mbi = (multiboot_info_t *)addr;

    Machine_EarlyInit();

    /* @r{Am I booted by a Multiboot-compliant boot loader?} */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
      kprintf("Invalid magic number: 0x%x\n", magic);
      //return;
    }

    /* @r{Print out the flags.} */
    kprintf("flags = 0x%x\n", (uint64_t) mbi->flags);

    /* @r{Are mem_* valid?} */
    if (CHECK_FLAG (mbi->flags, 0))
        kprintf("mem_lower = %uKB, mem_upper = %uKB\n",
                (unsigned) mbi->mem_lower, (unsigned) mbi->mem_upper);

    /* @r{Is boot_device valid?} */
    if (CHECK_FLAG (mbi->flags, 1))
        kprintf("boot_device = 0x%x\n", (unsigned) mbi->boot_device);
  
    /* @r{Is the command line passed?} */
    if (CHECK_FLAG (mbi->flags, 2))
        kprintf("cmdline = %s\n", (char *)(uintptr_t)mbi->cmdline);

    /* @r{Are mods_* valid?} */
    if (CHECK_FLAG (mbi->flags, 3))
    {
        multiboot_module_t *mod;
        int i;
      
        kprintf("mods_count = %d, mods_addr = 0x%x\n",
                (int) mbi->mods_count, (int) mbi->mods_addr);
        for (i = 0, mod = (multiboot_module_t *)(uintptr_t)mbi->mods_addr;
             i < mbi->mods_count;
             i++, mod++)
            kprintf(" mod_start = 0x%x, mod_end = 0x%x, cmdline = %s\n",
                    (unsigned) mod->mod_start,
                    (unsigned) mod->mod_end,
                    (char *)(uintptr_t) mod->cmdline);
    }

    /* @r{Bits 4 and 5 are mutually exclusive!} */
    if (CHECK_FLAG (mbi->flags, 4) && CHECK_FLAG (mbi->flags, 5))
    {
        kprintf("Both bits 4 and 5 are set.\n");
        return;
    }

    /* @r{Is the symbol table of a.out valid?} */
    if (CHECK_FLAG (mbi->flags, 4))
    {
        multiboot_aout_symbol_table_t *multiboot_aout_sym = &(mbi->u.aout_sym);
      
        kprintf("multiboot_aout_symbol_table: tabsize = 0x%0x\n"
                "                             strsize = 0x%x, addr = 0x%x\n",
                (unsigned) multiboot_aout_sym->tabsize,
                (unsigned) multiboot_aout_sym->strsize,
                (unsigned) multiboot_aout_sym->addr);
    }

    /* @r{Is the section header table of ELF valid?} */
    if (CHECK_FLAG (mbi->flags, 5))
    {
        multiboot_elf_section_header_table_t *multiboot_elf_sec = &(mbi->u.elf_sec);

        kprintf("multiboot_elf_sec: num = %u, size = 0x%x,"
                " addr = 0x%x, shndx = 0x%x\n",
                (unsigned) multiboot_elf_sec->num, (unsigned) multiboot_elf_sec->size,
                (unsigned) multiboot_elf_sec->addr, (unsigned) multiboot_elf_sec->shndx);
    }

    memRegionIdx = 0;

    /* @r{Are mmap_* valid?} */
    if (CHECK_FLAG (mbi->flags, 6))
    {
	multiboot_memory_map_t *mmap;

	kprintf("mmap_addr = 0x%x, mmap_length = 0x%x\n",
		(unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
	for (mmap = (multiboot_memory_map_t *)(uintptr_t) mbi->mmap_addr;
	    (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
	    mmap = (multiboot_memory_map_t *) ((unsigned long) mmap
				    + mmap->size + sizeof (mmap->size)))
	{
	    kprintf(" size = 0x%x, base_addr = 0x%llx,"
		    " length = 0x%llx, type = 0x%x\n",
		    (unsigned) mmap->size,
		    mmap->addr,
		    mmap->len,
		    (unsigned) mmap->type);
	    if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
		memRegionStart[memRegionIdx] = mmap->addr;
		memRegionLen[memRegionIdx] = mmap->len;
		memRegionIdx++;
	    }
	}
    }

    // Main initialization
    Machine_Init();

    return;
}

void
MachineBoot_AddMem()
{
    int i;
    uintptr_t initRamEnd = 32*1024*1024;

    for (i = 0; i < memRegionIdx; i++)
    {
	uintptr_t start = memRegionStart[i];
	uintptr_t len = memRegionLen[i];

	if (start + len < initRamEnd)
	    continue;

	if (start < initRamEnd) {
	    len = initRamEnd - start;
	    start = initRamEnd;
	}

	kprintf("AddRegion: %08llx %08llx\n", start, len);
	PAlloc_AddRegion(start + MEM_DIRECTMAP_BASE, len);
    }
}

