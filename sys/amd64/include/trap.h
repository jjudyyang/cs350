
#ifndef __TRAP_H__
#define __TRAP_H__

#define T_DE		0	/* Divide Error Exception */
#define T_DB		1	/* Debug Exception */
#define T_NMI		2	/* NMI Interrupt */
#define T_BP		3	/* Breakpoint Exception */
#define T_OF		4	/* Overflow Exception */
#define T_BR		5	/* BOUND Range Exceeded Exception */
#define T_UD		6	/* Invalid Opcode Exception */
#define T_NM		7	/* Device Not Available Exception */
#define T_DF		8	/* Double Fault Exception */
#define T_TS		10	/* Invalid TSS Exception */
#define T_NP		11	/* Segment Not Present */
#define T_SS		12	/* Stack Fault Exception */
#define T_GP		13	/* General Protection Exception */
#define T_PF		14	/* Page-Fault Exception */
#define T_MF		16	/* x87 FPU Floating-Point Error */
#define T_AC		17	/* Alignment Check Exception */
#define T_MC		18	/* Machine-Check Exception */
#define T_XF		19	/* SIMB Floating-Point Exception */
#define T_VE		20	/* Virtualization Exception */

#define T_CPU_LAST	T_VE

// IRQs
#define T_IRQ_BASE	32
#define T_IRQ_LEN	24
#define T_IRQ_MAX	(T_IRQ_BASE + T_IRQ_LEN - 1)

#define T_IRQ_TIMER	(T_IRQ_BASE + 0)
#define T_IRQ_KBD	(T_IRQ_BASE + 1)
#define T_IRQ_COM1	(T_IRQ_BASE + 4)
#define T_IRQ_MOUSE	(T_IRQ_BASE + 12)

// LAPIC Special Vectors
#define T_IRQ_SPURIOUS	(T_IRQ_BASE + 24)
#define T_IRQ_ERROR	(T_IRQ_BASE + 25)
#define T_IRQ_THERMAL	(T_IRQ_BASE + 26)

#define T_SYSCALL	60	/* System Call */
#define T_CROSSCALL	61	/* Cross Call (IPI) */
#define T_DEBUGIPI	62	/* Kernel Debugger Halt (IPI) */

#define T_UNKNOWN	63	/* Unknown Trap */

#define T_MAX		64

typedef struct TrapFrame
{
    uint64_t    r15;
    uint64_t    r14;
    uint64_t    r13;
    uint64_t    r12;
    uint64_t    r11;
    uint64_t    r10;
    uint64_t    r9;
    uint64_t    r8;
    uint64_t    rbp;
    uint64_t    rdi;
    uint64_t    rsi;
    uint64_t    rdx;
    uint64_t    rcx;
    uint64_t    rbx;
    uint64_t	ds;
    uint64_t    rax;

    uint64_t    vector;
    uint32_t    errcode;
    uint32_t    _unused0;
    uint64_t    rip;
    uint16_t    cs;
    uint16_t    _unused1;
    uint16_t    _unused2;
    uint16_t    _unused3;
    uint64_t    rflags;
    uint64_t    rsp;
    uint16_t    ss;
    uint16_t    _unused4;
    uint16_t    _unused5;
    uint16_t    _unused6;
} TrapFrame;

void Trap_Init();
void Trap_InitAP();
void Trap_Dump(TrapFrame *tf);
void Trap_Pop(TrapFrame *tf);

#endif /* __TRAP_H__ */

