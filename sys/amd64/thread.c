
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kconfig.h>
#include <sys/kassert.h>
#include <sys/kmem.h>
#include <sys/mp.h>
#include <sys/thread.h>
#include <machine/amd64.h>
#include <machine/amd64op.h>
#include <machine/trap.h>
#include <machine/pmap.h>

extern void ThreadKThreadEntry(TrapFrame *tf);
extern void switchstack(uint64_t *oldrsp, uint64_t rsp);

void
Thread_InitArch(Thread *thr)
{
    thr->arch.useFP = true;
}

void
Thread_SetupKThread(Thread *thr, void (*f)(),
		    uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{
    // Initialize stack
    uint64_t stacktop = thr->kstack + PGSIZE;
    ThreadArchStackFrame *sf;
    TrapFrame *tf;

    tf = (TrapFrame *)(stacktop - sizeof(*tf));
    sf = (ThreadArchStackFrame *)(stacktop - sizeof(*tf) - sizeof(*sf));
    thr->arch.rsp = (uint64_t)sf;

    memset(tf, 0, sizeof(*tf));
    memset(sf, 0, sizeof(*sf));

    // Setup thread exit function on stack

    sf->rip = (uint64_t)&ThreadKThreadEntry;
    sf->rdi = (uint64_t)tf;

    tf->ds = 0;
    tf->ss = 0; //SEL_KDS;
    tf->rsp = stacktop;
    tf->cs = SEL_KCS;
    tf->rip = (uint64_t)f;
    tf->rdi = (uint64_t)arg1;
    tf->rsi = (uint64_t)arg2;
    tf->rdx = (uint64_t)arg3;
    tf->rflags = RFLAGS_IF;
}

static void
ThreadEnterUserLevelCB(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{
    TrapFrame tf;

    memset(&tf, 0, sizeof(tf));
    tf.ds = SEL_UDS | 3;
    tf.rip = (uint64_t)arg1;
    tf.cs = SEL_UCS | 3;
    tf.rsp = (uint64_t)arg2 + MEM_USERSPACE_STKLEN - PGSIZE;
    tf.ss = SEL_UDS | 3;
    tf.rflags = RFLAGS_IF;
    tf.rdi = (uint64_t)arg3; /* Userspace Argument */

    Trap_Pop(&tf);
}

void
Thread_SetupUThread(Thread *thr, uintptr_t rip, uintptr_t arg)
{
    Thread_SetupKThread(thr, ThreadEnterUserLevelCB, rip,
			thr->ustack, arg);
}

extern TaskStateSegment64 TSS[MAX_CPUS];

void
Thread_SwitchArch(Thread *oldthr, Thread *newthr)
{
    /*
     * Save and restore floating point and vector CPU state using the fxsave 
     * and fxrstor instructions.
     */
    if (oldthr->arch.useFP)
    {
	fxsave(&oldthr->arch.xsa);
    }

    if (newthr->arch.useFP)
    {
	fxrstor(&newthr->arch.xsa);
    }

    clts();

    // Jump to trapframe
    switchstack(&oldthr->arch.rsp, newthr->arch.rsp);

    // Set new RSP0
    TSS[CPU()].rsp0 = oldthr->kstack + 4096;
}

