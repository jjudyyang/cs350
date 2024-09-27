
#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kconfig.h>
#include <sys/kdebug.h>
#include <sys/mp.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>
#include <machine/atomic.h>
#include <machine/trap.h>
#include <machine/lapic.h>
#include <machine/mp.h>

TrapFrame *frames[MAX_CPUS];

volatile static uint64_t debugLock = 0;
volatile static uint64_t debugCmd = 0;
volatile static uint64_t debugHalted = 0;

void
Debug_HaltCPUs()
{
    debugCmd = 0;

    if (MP_GetCPUs() == 1)
	return;

    LAPIC_BroadcastNMI(T_DEBUGIPI);

    // Wait for processors to enter
    while (debugHalted < (MP_GetCPUs() - 1)) {
	pause();
    }
}

void
Debug_ResumeCPUs()
{
    debugCmd = 1;

    // Wait for processors to resume
    while (debugHalted > 0) {
	pause();
    }
}

void
Debug_HaltIPI(TrapFrame *tf)
{
    MP_SetState(CPUSTATE_HALTED);
    __sync_fetch_and_add(&debugHalted, 1);

    frames[CPU()] = tf;

    while (debugCmd == 0) {
	pause();
    }

    __sync_fetch_and_sub(&debugHalted, 1);
    MP_SetState(CPUSTATE_BOOTED);
}

void
Debug_Breakpoint(TrapFrame *tf)
{
    frames[CPU()] = tf;

    // Should probably force all cores into the debugger
    while(atomic_swap_uint64(&debugLock, 1) == 1) {
	// Wait to acquire debugger lock
    }

    // Stop all processors
    Debug_HaltCPUs();

    // Enter prompt
    Debug_Prompt();

    // Resume all processors
    Debug_ResumeCPUs();
    atomic_set_uint64(&debugLock, 0);
}

static void
Debug_Registers(int argc, const char *argv[])
{
    TrapFrame *tf = frames[CPU()];

    if (argc == 2) {
	int cpuNo = Debug_StrToInt(argv[1]);
	if (cpuNo >= MAX_CPUS) {
	    kprintf("Invalid CPU number\n");
	    return;
	}
	tf = frames[cpuNo];
    }

    kprintf("Interrupt %d Error Code: %016llx\n",
            tf->vector, tf->errcode);
    kprintf("cr0: %016llx cr2: %016llx\n",
            read_cr0(), read_cr2());
    kprintf("cr3: %016llx cr4: %016llx\n",
            read_cr3(), read_cr4());
    kprintf("dr0: %016llx dr1: %016llx dr2: %016llx\n",
            read_dr0(), read_dr1(), read_dr2());
    kprintf("dr3: %016llx dr6: %016llx dr7: %016llx\n",
            read_dr3(), read_dr6(), read_dr7());
    kprintf("rip: %04x:%016llx rsp: %04x:%016llx\n",
            tf->cs, tf->rip, tf->ss, tf->rsp);
    kprintf("rflags: %016llx ds: %04x es: %04x fs: %04x gs: %04x\n",
            tf->rflags, read_ds(), read_es(), read_fs(), read_gs());
    kprintf("rax: %016llx rbx: %016llx rcx: %016llx\n",
            tf->rax, tf->rbx, tf->rcx);
    kprintf("rdx: %016llx rsi: %016llx rdi: %016llx\n",
            tf->rdx, tf->rsi, tf->rdi);
    kprintf("rbp: %016llx r8:  %016llx r9:  %016llx\n",
            tf->rbp, tf->r8, tf->r9);
    kprintf("r10: %016llx r11: %016llx r12: %016llx\n",
            tf->r10, tf->r11, tf->r12);
    kprintf("r13: %016llx r14: %016llx r15: %016llx\n",
            tf->r13, tf->r14, tf->r15);
}

REGISTER_DBGCMD(registers, "Show CPU registers", Debug_Registers);

static void
Debug_Backtrace(int argc, const char *argv[])
{
    TrapFrame *tf = frames[CPU()];
    uint64_t *ptr;
    uint64_t rip;
    uint64_t rbp;

    if (argc == 2) {
	int cpuNo = Debug_StrToInt(argv[1]);
	if (cpuNo >= MAX_CPUS) {
	    kprintf("Invalid CPU number\n");
	    return;
	}
	tf = frames[cpuNo];
    }

    rip = tf->rip;
    rbp = tf->rbp;

    kprintf("%-16s %-16s\n", "IP Pointer", "Base Pointer");
    while (3) {
	kprintf("%016llx %016llx\n", rip, rbp);
	ptr = (uint64_t *)rbp;
	if (rbp == 0ULL || rip == 0ULL) {
	    break;
	}
	rbp = ptr[0];
	rip = ptr[1];
    }
}

REGISTER_DBGCMD(backtrace, "Print backtrace", Debug_Backtrace);

static void
Debug_SetBreakpoint(int argc, const char *argv[])
{
    if (argc != 2) {
	kprintf("Usage: bkpt ADDR\n");
	kprintf("Provide an address for the breakpoint!\n");
	return;
    }

    uint64_t addr = Debug_StrToInt(argv[1]);

    uint32_t flags = read_dr7();

    if (!(flags & DR7_DR0G)) {
	write_dr0(addr);
	write_dr7(flags | DR7_DR0G);
    } else if (!(flags & DR7_DR1G)) {
	write_dr1(addr);
	write_dr7(flags | DR7_DR1G);
    } else if (!(flags & DR7_DR2G)) {
	write_dr2(addr);
	write_dr7(flags | DR7_DR2G);
    } else if (!(flags & DR7_DR3G)) {
	write_dr3(addr);
	write_dr7(flags | DR7_DR3G);
    } else {
	kprintf("No more breakpoints left!\n");
    }
}

REGISTER_DBGCMD(bkpt, "Set breakpoint", Debug_SetBreakpoint);

static void
Debug_ClearBreakpoint(int argc, const char *argv[])
{
    if (argc != 2) {
	kprintf("Usage: clrbkpt 0-3\n");
	kprintf("Specify which breakpoint to clear!\n");
	return;
    }

    uint32_t flags = read_dr7();
    switch (argv[1][0]) {
	case '0':
	    flags &= ~DR7_DR0G;
	    break;
	case '1':
	    flags &= ~DR7_DR1G;
	    break;
	case '2':
	    flags &= ~DR7_DR2G;
	    break;
	case '3':
	    flags &= ~DR7_DR3G;
	    break;
	default:
	    kprintf("Specify a breakpoint between 0-3\n");
    }

    write_dr7(flags);
}

REGISTER_DBGCMD(clrbkpt, "Clear breakpoint", Debug_ClearBreakpoint);

static void
Debug_ListBreakpoints(int argc, const char *argv[])
{
    uint32_t flags = read_dr7();

    if (flags & DR7_DR0G) {
	kprintf("DR0: 0x%lx\n", read_dr0());
    }
    if (flags & DR7_DR1G) {
	kprintf("DR1: 0x%lx\n", read_dr1());
    }
    if (flags & DR7_DR2G) {
	kprintf("DR2: 0x%lx\n", read_dr2());
    }
    if (flags & DR7_DR3G) {
	kprintf("DR3: 0x%lx\n", read_dr3());
    }
}

REGISTER_DBGCMD(bkpts, "List breakpoint", Debug_ListBreakpoints);

static void
Debug_Reboot(int argc, const char *argv[])
{
    /*
     * Triple fault the core by loading a non-canonical address into CR3.  We 
     * should use ACPI, or even the keyboard controller if it exists.
     */
    write_cr3(0x8000000000000000ULL);
}

REGISTER_DBGCMD(reboot, "Reboot computer", Debug_Reboot);

