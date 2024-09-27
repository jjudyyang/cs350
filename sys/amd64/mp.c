
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/kconfig.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/ktime.h>
#include <sys/mp.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>
#include <machine/pmap.h>
#include <machine/lapic.h>
#include <machine/mp.h>
#include <machine/trap.h>

extern uint8_t mpstart_begin[];
extern uint8_t mpstart_end[];

extern AS systemAS;

#define MP_WAITTIME	250000000ULL

typedef struct CrossCallFrame {
    CrossCallCB		cb;
    void		*arg;
    volatile int	count;
    volatile int	done[MAX_CPUS];
    volatile int	status[MAX_CPUS];
} CrossCallFrame;

const char *CPUStateToString[] = {
    "NOT PRESENT",
    "BOOTED",
    "HALTED",
};

typedef struct CPUState {
    int			state;
    UnixEpochNS		heartbeat;
    CrossCallFrame	*frame;
} CPUState;

volatile static bool booted;
volatile static int lastCPU;
volatile static CPUState cpus[MAX_CPUS];

static int
MPBootAP(int procNo)
{
    UnixEpochNS startTS, stopTS;
    /*
     * Arguments to mpstart are stored at 0x6F00
     * arg[0] = CR3
     * arg[1] = RSP
     */
    volatile uint64_t *args = (uint64_t *)DMPA2VA(0x6F00);

    kprintf("Starting processor %d\n", procNo);
    memcpy((void *)DMPA2VA(0x7000), mpstart_begin, mpstart_end - mpstart_begin);

    args[0] = DMVA2PA((uint64_t)systemAS.root);
    args[1] = PGSIZE + (uint64_t)PAlloc_AllocPage();

    kprintf("CR3: %016llx RSP: %016llx\n", args[0], args[1]);

    booted = 0;
    LAPIC_StartAP(procNo, 0x7000);

    startTS = KTime_GetEpochNS();
    while (1) {
	if (booted == 1)
	    break;

	stopTS = KTime_GetEpochNS();
	if ((stopTS - startTS) > MP_WAITTIME) {
	    kprintf("Processor %d did not respond in %d ms\n",
		    procNo, MP_WAITTIME / 1000000ULL);
	    PAlloc_Release((void *)(args[1] - PGSIZE));
	    return -1;
	}
    }

    return 0;
}

void
MP_Init()
{
    int i;
    kprintf("Booting on CPU %u\n", CPU());

    cpus[CPU()].state = CPUSTATE_BOOTED;
    cpus[CPU()].frame = NULL;

    for (i = 1; i < MAX_CPUS; i++) {
	cpus[i].state = CPUSTATE_NOT_PRESENT;
	cpus[i].frame = NULL;
    }

    /*
     * XXX: We really should read from the MP Table, but this appears to be 
     * reliable for now.
     */
    lastCPU = 0;
    for (i = 1; i < MAX_CPUS; i++) {
	if (MPBootAP(i) < 0)
	    break;

	lastCPU = i;
    }
    lastCPU++;
}

void
MP_InitAP()
{
    kprintf("AP %d booted!\n", CPU());
    cpus[CPU()].state = CPUSTATE_BOOTED;
    booted = 1;
}

void
MP_SetState(int state)
{
    ASSERT(state > 0 && state <= CPUSTATE_MAX);
    cpus[CPU()].state = state;
}

int
MP_GetCPUs()
{
    return lastCPU;
}

void
MP_CrossCallTrap()
{
    int c;

    cpuid(0, 0, 0, 0, 0);

    Critical_Enter();

    for (c = 0; c <= lastCPU; c++) {
	CrossCallFrame *frame = cpus[c].frame;
	if (frame == NULL)
	    continue;

	if (frame->done[CPU()] == 1)
	    continue;

	frame->status[CPU()] = (frame->cb)(frame->arg);
	frame->done[CPU()] = 1;

	// Increment
	__sync_add_and_fetch(&frame->count, 1);
    }

    Critical_Exit();
}

// XXX: The thread should not be migrated in the middle of this call.
int
MP_CrossCall(CrossCallCB cb, void *arg)
{
    volatile CrossCallFrame frame;

    // Setup frame
    memset((void *)&frame, 0, sizeof(frame));
    frame.cb = cb;
    frame.arg = arg;
    frame.count = 1;

    Critical_Enter();

    cpus[CPU()].frame = (CrossCallFrame *)&frame;
    cpuid(0, 0, 0, 0, 0);

    if (LAPIC_Broadcast(T_CROSSCALL) < 0)
	return -1;

    // Run on the local CPU
    frame.status[CPU()] = cb(arg);
    frame.done[CPU()] = 1;

    // Wait for all to respond
    while (frame.count < lastCPU) {
	// Check for timeout

	// XXX: Should dump the crosscall frame
    }
    cpus[CPU()].frame = NULL;
    cpuid(0, 0, 0, 0, 0);

    Critical_Exit();

    return 0;
}

static int
MPPing(void *arg)
{
    //kprintf("CPU %d Ack\n", CPU());
    return 0;
}

static void
Debug_CrossCall(int argc, const char *argv[])
{
    int i;
    UnixEpochNS startTS, stopTS;

    startTS = KTime_GetEpochNS();
    for (i = 0; i < 32; i++) {
	MP_CrossCall(&MPPing, NULL);
    }
    stopTS = KTime_GetEpochNS();

    // XXX: Print min and max
    kprintf("Average CrossCall Latency: %llu ns\n",
	    (stopTS - startTS) / 32ULL);

    return;
}

REGISTER_DBGCMD(crosscall, "Ping crosscall", Debug_CrossCall);

static void
Debug_CPUS(int argc, const char *argv[])
{
    int c;

    for (c = 0; c < MAX_CPUS; c++) {
	if (cpus[c].state != CPUSTATE_NOT_PRESENT) {
	    kprintf("CPU %d: %s\n", c, CPUStateToString[cpus[c].state]);
	}
    }
}

REGISTER_DBGCMD(cpus, "Show MP information", Debug_CPUS);

static void
Debug_CPU(int argc, const char *argv[])
{
    kprintf("CPU %d\n", CPU());
}

REGISTER_DBGCMD(cpu, "Current CPU number", Debug_CPU);

