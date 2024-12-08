/*
 * Copyright (c) 2006-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/sysctl.h>
#include <sys/kmem.h>
#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/elf64.h>

#include <machine/amd64.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <sys/thread.h>
#include <sys/spinlock.h>
#include <sys/loader.h>

#include <sys/vfs.h>

extern Handle *Console_OpenHandle();

/**
 * Loader_CheckHeader --
 *
 * Check that the program has a valid ELF header.
 */
bool
Loader_CheckHeader(const Elf64_Ehdr *ehdr)
{
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
	ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
	ehdr->e_ident[EI_MAG3] != ELFMAG3)
	return false;

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
	return false;
    }

    if (ehdr->e_machine != EM_AMD64) {
	return false;
    }

    return true;
}

/**
 * LoaderLoadSegment --
 *
 * Loads a single segment into the target address space.  This function loads a 
 * single page at a time because it has to lookup the address mappings through 
 * the page tables.
 */
static void
LoaderLoadSegment(AS *as, VNode *vn, uintptr_t vaddr,
		 uintptr_t offset, uintptr_t len)
{
    void *raddr;

    if ((vaddr % PGSIZE) != 0) {
	uintptr_t maxlen = PGSIZE - (vaddr % PGSIZE);
	uintptr_t rlen = maxlen < len ? maxlen : len;

	raddr = (void *)DMPA2VA(PMap_Translate(as, vaddr));
	VFS_Read(vn, raddr, offset, rlen);
	vaddr += rlen;
	offset += rlen;
	len -= rlen;
    }

    while (len > PGSIZE) {
	raddr = (void *)DMPA2VA(PMap_Translate(as, vaddr));
	VFS_Read(vn, raddr, offset, PGSIZE);
	vaddr += PGSIZE;
	offset += PGSIZE;
	len -= PGSIZE;
    }

    if (len > 0) {
	raddr = (void *)DMPA2VA(PMap_Translate(as, vaddr));
	VFS_Read(vn, raddr, offset, len);
    }
}

/**
 * LoaderZeroSegment --
 *
 * Zeroes a segment of memory in the target address space.  This is done one 
 * page a time while translating the virtual address to physical.
 */
static void
LoaderZeroSegment(AS *as, uintptr_t vaddr, uintptr_t len)
{
    void *raddr;

    if ((vaddr % PGSIZE) != 0) {
	uintptr_t maxlen = PGSIZE - (vaddr % PGSIZE);
	uintptr_t rlen = maxlen < len ? maxlen : len;

	raddr = (void *)DMPA2VA(PMap_Translate(as, vaddr));
	memset(raddr, 0, rlen);
	vaddr += rlen;
	len -= rlen;
    }

    while (len > PGSIZE) {
	raddr = (void *)DMPA2VA(PMap_Translate(as, vaddr));
	memset(raddr, 0, PGSIZE);
	vaddr += PGSIZE;
	len -= PGSIZE;
    }

    if (len > 0) {
	raddr = (void *)DMPA2VA(PMap_Translate(as, vaddr));
	memset(raddr, 0, len);
    }
}

/**
 * Loader_Load --
 *
 * Load the ELF binary into the process belonging to the thread.
 */
bool
Loader_Load(Thread *thr, VNode *vn, void *buf, uint64_t len)
{
    int i;
    const Elf64_Ehdr *ehdr;
    const Elf64_Phdr *phdr;
    AS *as = thr->space;

    ehdr = (const Elf64_Ehdr *)(buf);
    phdr = (const Elf64_Phdr *)(buf + ehdr->e_phoff);

    if (!Loader_CheckHeader(ehdr)) {
	Log(loader, "Not a valid executable!\n");
	return false;
    }

    Log(loader, "%8s %16s %8s %8s\n", "Offset", "VAddr", "FileSize", "MemSize");
    for (i = 0; i < ehdr->e_phnum; i++)
    {
	ASSERT(phdr[i].p_type != PT_DYNAMIC);
	if (phdr[i].p_type == PT_LOAD) {
	    uint64_t va = phdr[i].p_vaddr;
	    uint64_t memsz = phdr[i].p_memsz;
	    Log(loader, "%08llx %016llx %08llx %08llx\n", phdr[i].p_offset,
		    phdr[i].p_vaddr, phdr[i].p_filesz, phdr[i].p_memsz);

	    // Make sure it is page aligned
	    va = va & ~(uint64_t)PGMASK;
	    memsz += phdr[i].p_vaddr - va;

	    Log(loader, "AllocMap %016llx %08llx\n", va, memsz);
	    if (!PMap_AllocMap(as, va, memsz, PTE_W)) {
		// XXX: Cleanup!
		ASSERT(false);
		return false;
	    }
	}
    }

    PMap_AllocMap(as, MEM_USERSPACE_STKBASE, MEM_USERSPACE_STKLEN, PTE_W);

    /* XXXFILLMEIN: Load the ELF segments. */
    for (i = 0; i < ehdr->e_phnum; i++) {
       if (phdr[i].p_type == PT_LOAD) {
           LoaderLoadSegment(as, vn, phdr[i].p_vaddr, phdr[i].p_offset, phdr[i].p_filesz);
           LoaderZeroSegment(as, phdr[i].p_vaddr + phdr[i].p_filesz, phdr[i].p_memsz - phdr[i].p_filesz);
       }
   }

    /* Save the process entry point (i.e., _start) */
    thr->proc->entrypoint = ehdr->e_entry;

    return true;
}

/**
 * Loader_LoadInit --
 *
 * The init process is created from the execution kernel thread that 
 * initializes the system.  This function initializes the thread and process 
 * state then loads the init binary.
 */
void
Loader_LoadInit()
{
    int status;
    void *pg;
    VNode *initvn;

    pg = PAlloc_AllocPage();
    if (!pg)
	Panic("Not enough memory!");

    initvn = VFS_Lookup("/sbin/init");
    status = VFS_Open(initvn);
    if (status < 0)
	Panic("Loading init process failed!");
    status = VFS_Read(initvn, pg, 0, 1024);
    if (status < 0)
	Panic("Reading init process failed!");

    Thread *thr = Sched_Current();

    // Open stdin/out/err
    Handle *handle = Console_OpenHandle();
    Handle_Add(thr->proc, handle);
    handle = Console_OpenHandle();
    Handle_Add(thr->proc, handle);
    handle = Console_OpenHandle();
    Handle_Add(thr->proc, handle);

    /*
     * Load init binary
     */
    Loader_Load(thr, initvn, pg, 1024);

    VFS_Close(initvn);

    Log(loader, "Jumping to userspace\n");

    /*
     * Reload the page tables for the current process
     */
    PMap_LoadAS(thr->space); // Reload CR3

    /*
     * Pass in zero arguments with null pointers to init
     */
    uintptr_t ap[3];
    ap[0] = 0;
    ap[1] = 0;
    ap[2] = 0xDEADBEEF;
    uintptr_t rsp = MEM_USERSPACE_STKTOP - PGSIZE;

    Copy_Out(&ap[0], rsp, sizeof(uintptr_t)*3);

    /*
     * The last step is to return into userspace handing control to init.  We 
     * create a valid trap frame and return into userspace using Trap_Pop().
     */
    TrapFrame tf;
    memset(&tf, 0, sizeof(tf));
    tf.ds = SEL_UDS | 3;
    tf.rip = thr->proc->entrypoint;
    tf.cs = SEL_UCS | 3;
    tf.rsp = rsp;
    tf.ss = SEL_UDS | 3;
    tf.rflags = RFLAGS_IF;
    tf.rdi = rsp;
    Trap_Pop(&tf);

    /*
     * We should never reach this point!
     */
    Panic("Unreachable: Trap_Pop() returned!\n");
}

//reuploaded

