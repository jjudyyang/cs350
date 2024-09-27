
#include <stdbool.h>
#include <stdint.h>

#include <sys/kconfig.h>
#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/ktime.h>
#include <sys/spinlock.h>
#include <sys/irq.h>
#include <sys/syscall.h>
#include <sys/mp.h>

#include <machine/amd64.h>
#include <machine/lapic.h>
#include <machine/trap.h>
#include <machine/mp.h>

#include <sys/thread.h>

extern uint64_t trap_table[T_MAX];
extern void trap_pop(TrapFrame *tf);
extern void Debug_Breakpoint(TrapFrame *tf);
extern void Debug_HaltIPI(TrapFrame *tf);
extern void KTimer_Process();

static InteruptGate64 idt[256];
static PseudoDescriptor idtdesc;
static uint64_t intStats[256];

void
Trap_Init()
{
    int i;

    kprintf("Initializing IDT... ");

    for (i = 0; i < T_MAX; i++) {
        idt[i].pc_low = trap_table[i] & 0x0000ffff;
        idt[i].pc_mid = (trap_table[i] >> 16) & 0x0000ffff;
        idt[i].pc_high = trap_table[i] >> 32;

        idt[i].cs = 0x0008;
        idt[i].type = 0x8E;

        idt[i].ist = 0x00;
        idt[i]._unused1 = 0x00000000;
    }

    for (; i < 256; i++) {
        idt[i].pc_low = trap_table[63] & 0x0000ffff;
        idt[i].pc_mid = (trap_table[63] >> 16) & 0x0000ffff;
        idt[i].pc_high = trap_table[63] >> 32;

        idt[i].cs = 0x0008;
        idt[i].type = 0x8E;

        idt[i].ist = 0;
        idt[i]._unused1 = 0;
    }

    // Double fault handler
    idt[T_NMI].ist = 0x01;
    idt[T_DF].ist = 0x01;
    idt[T_GP].ist = 0x01;
    idt[T_NP].ist = 0x01;
    idt[T_SS].ist = 0x01;

    // Enable Breakpoint and Syscall from Userspace
    idt[T_DB].type = 0xEE;
    idt[T_BP].type = 0xEE;
    idt[T_SYSCALL].type = 0xEE;

    idtdesc.off = (uint64_t)&idt;
    idtdesc.lim = sizeof(idt) - 1;

    lidt(&idtdesc);

    // Zero out interrupt stats
    for (i = 0; i < 256; i++) {
	intStats[i] = 0;
    }

    kprintf("Done!\n");
}

void
Trap_InitAP()
{
    lidt(&idtdesc);
}

void
Trap_Dump(TrapFrame *tf)
{
    kprintf("CPU %d\n", CPU());
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

void
Trap_StackDump(TrapFrame *tf)
{
    uint64_t rsp;
    uint64_t *data;

    // XXX: This should use safe copy
    for (rsp = tf->rsp; (rsp & 0xFFF) != 0; rsp += 8) {
	data = (uint64_t *)rsp;
	kprintf("%016llx: %016llx\n", rsp, *data);
    }
}

extern int copy_unsafe(void *to, void *from, uintptr_t len);
extern void copy_unsafe_done(void);
extern void copy_unsafe_fault(void);

extern int copystr_unsafe(void *to, void *from, uintptr_t len);
extern void copystr_unsafe_done(void);
extern void copystr_unsafe_fault(void);

void
trap_entry(TrapFrame *tf)
{
    // XXX: USE ATOMIC!
    intStats[tf->vector]++;

    // Debug NMI
    if (tf->vector == T_NMI) {
	Critical_Enter();
	kprintf("Kernel Fault: Vector %d\n", tf->vector);
	Trap_Dump(tf);
	Debug_Breakpoint(tf);
	return;
    }

    // Kernel
    if (tf->cs == SEL_KCS)
    {
	// Kernel Debugger
	if (tf->vector == T_BP || tf->vector == T_DE) {
	    Debug_Breakpoint(tf);
	    return;
	}

	// User IO
	if ((tf->vector == T_PF) &&
	    (tf->rip >= (uint64_t)&copy_unsafe) &&
	    (tf->rip <= (uint64_t)&copy_unsafe_done)) {
	    kprintf("Faulted in copy_unsafe\n");
	    tf->rip = (uint64_t)&copy_unsafe_fault;
	    return;
	}

	// User IO
	if ((tf->vector == T_PF) &&
	    (tf->rip >= (uint64_t)&copystr_unsafe) &&
	    (tf->rip <= (uint64_t)&copystr_unsafe_done)) {
	    kprintf("Faulted in copystr_unsafe\n");
	    tf->rip = (uint64_t)&copystr_unsafe_fault;
	    return;
	}

	// Halt on kernel errors
	if (tf->vector <= T_CPU_LAST)
	{
	    Critical_Enter();
	    kprintf("Kernel Fault: Vector %d\n", tf->vector);
	    Trap_Dump(tf);
	    Debug_Breakpoint(tf);
	    while (1)
		hlt();
	}
    }

    // User space exceptions
    switch (tf->vector)
    {
	case T_DE:
	    kprintf("Divide by Zero\n");
	    return;
	case T_DB:
	case T_BP:
	case T_UD: {
	    kprintf("Userlevel breakpoint\n");
	    Debug_Breakpoint(tf);
	    return;
	}
	case T_PF: {
	    Trap_Dump(tf);
	    Trap_StackDump(tf);
	    Debug_Breakpoint(tf);
	}
	case T_SYSCALL: {
	    VLOG(syscall, "Syscall %016llx\n", tf->rdi);
	    tf->rax = Syscall_Entry(tf->rdi, tf->rsi, tf->rdx, tf->rcx, tf->r8, tf->r9);
	    VLOG(syscall, "Return %016llx\n", tf->rax);
	    return;
	}
    }

    // IRQs
    if (tf->vector >= T_IRQ_BASE && tf->vector <= T_IRQ_MAX)
    {
	LAPIC_SendEOI();
	IRQ_Handler(tf->vector - T_IRQ_BASE);
	if (tf->vector == T_IRQ_TIMER) {
	    KTimer_Process();
	    Sched_Scheduler();
	}

        return;
    }

    // Debug IPI
    if (tf->vector == T_DEBUGIPI) {
	Debug_HaltIPI(tf);
	LAPIC_SendEOI();
    }

    // Cross calls
    if (tf->vector == T_CROSSCALL)
    {
	MP_CrossCallTrap();
	LAPIC_SendEOI();
	return;
    }

    // LAPIC Special Vectors
    if (tf->vector == T_IRQ_SPURIOUS)
    {
	kprintf("Spurious Interrupt!\n");
	return;
    }
    if (tf->vector == T_IRQ_ERROR)
    {
	kprintf("LAPIC Error!\n");
	while (1)
	    hlt();
    }
    if (tf->vector == T_IRQ_THERMAL)
    {
	kprintf("Thermal Error!\n");
	while (1)
	    hlt();
    }

    kprintf("Unhandled Interrupt 0x%x!\n", tf->vector);
    Trap_Dump(tf);
    while (1)
	hlt();
}

static void
Debug_Traps(int argc, const char *argv[])
{
    int i;

    kprintf("Trap    Interrupts    Trap    Interrupts\n");
    for (i = 0; i < T_MAX / 2; i++)
    {
	kprintf("%-4d    %-12d  %-4d    %-12d\n",
		i, intStats[i],
		T_MAX / 2 + i, intStats[T_MAX / 2 + i]);
    }
}

REGISTER_DBGCMD(traps, "Print trap statistics", Debug_Traps);

