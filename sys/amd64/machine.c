#include <stdbool.h>
#include <stdint.h>

#include <sys/kconfig.h>
#include <sys/kassert.h>
#include <sys/kmem.h>
#include <sys/mp.h>
#include <sys/irq.h>
#include <sys/spinlock.h>

#include <machine/amd64.h>
#include <machine/ioapic.h>
#include <machine/lapic.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/mp.h>

#include <sys/thread.h>
#include <sys/disk.h>
#include <sys/bufcache.h>
#include <sys/vfs.h>
#include <sys/elf64.h>

#include "../dev/console.h"

extern void KTime_Init();
extern void KTimer_Init();
extern void RTC_Init();
extern void PS2_Init();
extern void PCI_Init();
extern void IDE_Init();
extern void MachineBoot_AddMem();
extern void Loader_LoadInit();
extern void PAlloc_LateInit();

#define GDT_MAX 8

static SegmentDescriptor GDT[MAX_CPUS][GDT_MAX];
static PseudoDescriptor GDTDescriptor[MAX_CPUS];
TaskStateSegment64 TSS[MAX_CPUS];

static char df_stack[4096];
 
/**
 * Machine_GDTInit --
 *  
 * Configures the Global Descriptor Table (GDT) that lists all segments and 
 * privilege levels in x86.  64-bit mode uses flat 64-bit segments and doesn't 
 * support offsets and limits except for the special FS/GS segment registers.  
 * We create four segments for the kernel code/data and user code/data.
 */
static void
Machine_GDTInit()
{
    uint64_t offset;
    uint64_t tmp;
    int c = CPU();

    kprintf("Initializing GDT... "); // Caused pagefault??

    GDT[c][0] = 0x0;
    GDT[c][1] = 0x00AF9A000000FFFFULL; /* Kernel CS */
    GDT[c][2] = 0x00CF92000000FFFFULL; /* Kernel DS */
    GDT[c][3] = 0x0;

    // TSS
    offset = (uint64_t)&TSS[c];
    GDT[c][4] = sizeof(TaskStateSegment64);
    tmp = offset & 0x00FFFFFF;
    GDT[c][4] |= (tmp << 16);
    tmp = offset & 0xFF000000;
    GDT[c][4] |= (tmp << 56);
    GDT[c][4] |= 0x89ULL << 40;
    GDT[c][5] = offset >> 32;

    GDT[c][6] = 0x00AFFA000000FFFFULL; /* User CS */
    GDT[c][7] = 0x00CFF2000000FFFFULL; /* User DS */

    GDTDescriptor[c].off = (uint64_t)&GDT[c];
    GDTDescriptor[c].lim = 8*GDT_MAX - 1;

    lgdt(&GDTDescriptor[c]);

    kprintf("Done!\n");
}

/**
 * Machine_TSSInit --
 *
 * Configures the Task State Segment (TSS) that specifies the kernel stack 
 * pointer during an interrupt.
 */
static void
Machine_TSSInit()
{
    int c = CPU();

    kprintf("Initializing TSS... ");

    TSS[c]._unused0 = 0;
    TSS[c]._unused1 = 0;
    TSS[c]._unused2 = 0;
    TSS[c]._unused3 = 0;
    TSS[c]._unused4 = 0;
    TSS[c].ist1 = ((uint64_t)&df_stack) + 4096;
    TSS[c].ist2 = 0x0;
    TSS[c].ist3 = 0x0;
    TSS[c].ist4 = 0x0;
    TSS[c].ist5 = 0x0;
    TSS[c].ist6 = 0x0;
    TSS[c].ist7 = 0x0;
    TSS[c].rsp0 = ((uint64_t)&df_stack) + 4096;
    TSS[c].rsp1 = 0;
    TSS[c].rsp2 = 0;

    ltr(SEL_TSS);

    kprintf("Done!\n");
}

/**
 * Machine_SyscallInit --
 *
 * Configure the model specific registers (MSRs) that specify how to transfer 
 * control to the operating system when the system call instruction is invoked.
 */
static void
Machine_SyscallInit()
{
    kprintf("Initializing Syscall... ");

    wrmsr(MSR_STAR, (uint64_t)SEL_KCS << 32 | (uint64_t)SEL_UCS << 48);
    wrmsr(MSR_LSTAR, 0);
    wrmsr(MSR_CSTAR, 0);
    wrmsr(MSR_SFMASK, 0);

    kprintf("Done!\n");
}

/**
 * Machine_EarlyInit --
 *
 * Initializes early kernel state.
 */
void
Machine_EarlyInit()
{
    Spinlock_EarlyInit();
    Critical_Init();
    Critical_Enter();
    WaitChannel_EarlyInit();
    Console_Init();
    PAlloc_Init();
}

static void
Machine_IdleThread(void *test)
{
    while (1) { enable_interrupts(); hlt(); }
}

/**
 * Machine_Init --
 *
 * At this point the assembly startup code has setup temporary processor data 
 * structures sufficient to execute C code and make it through this 
 * initialization routine.
 */
void Machine_Init()
{
    /*
     * Initialize Processor State
     */
    Machine_GDTInit();
    Machine_TSSInit();
    Trap_Init();
    Machine_SyscallInit();
    clts(); // Enable FPU/XMM

    /*
     * Initialize Memory Allocation and Virtual Memory
     */
    PAlloc_AddRegion(DMPA2VA(16*1024*1024), 16*1024*1024);
    PMap_Init();
    XMem_Init();
    PAlloc_LateInit();
    MachineBoot_AddMem();

    /*
     * Initialize Time Keeping
     */
    KTime_Init();
    RTC_Init(); // Finishes initializing KTime

    /*
     * Initialize Interrupts
     */
    IRQ_Init();
    LAPIC_Init();
    IOAPIC_Init();
    IOAPIC_Enable(0); // Enable timer interrupts
    Thread_Init();

    KTimer_Init(); // Depends on RTC and KTime

    /*
     * Initialize Additional Processors
     */
    MP_Init();

    /*
     * Initialize Basic Devices
     */
    PS2_Init(); // PS2 Keyboard
    PCI_Init(); // PCI BUS
    IDE_Init(); // IDE Disk Controller
    BufCache_Init();

    /*
     * Open the primary disk and mount the root file system
     */
    Disk *root = Disk_GetByID(0, 0);
    if (!root)
	Panic("No boot disk!");
    VFS_MountRoot(root);

    Critical_Exit();

    /*
     * Create the idle thread
     */
    Thread *thr = Thread_KThreadCreate(&Machine_IdleThread, NULL);
    if (thr == NULL) {
	kprintf("Couldn't create idle thread!\n");
    }
    Sched_SetRunnable(thr);

    /*
     * Load the init processor
     */
    Loader_LoadInit();

    breakpoint();
}

/**
 * Machine_InitAP --
 *
 * Shorter initialization routine for co-processors.
 */
void Machine_InitAP()
{
    Critical_Enter();

    // Setup CPU state
    Trap_InitAP();
    PMap_InitAP();
    Machine_GDTInit();
    Machine_TSSInit();
    Machine_SyscallInit();
    clts(); // Enable FPU/XMM

    // Setup LAPIC
    LAPIC_Init();

    // Boot processor
    MP_InitAP();
    Thread_InitAP();
    Critical_Exit();

    Machine_IdleThread(NULL);
}

