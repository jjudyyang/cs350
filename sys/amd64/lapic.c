/*
 * LAPIC
 */

#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>
#include <machine/pmap.h>
#include <machine/trap.h>

#define CPUID_FLAG_APIC             0x100

#define IA32_APIC_BASE_MSR          0x1B
#define IA32_APIC_BASE_MSR_BSP      0x100
#define IA32_APIC_BASE_MSR_ENABLE   0x800

#define LAPIC_ID            0x0020 /* CPU ID */
#define LAPIC_VERSION       0x0030 /* Version */
#define LAPIC_VERSION_LVTMASK		0x00FF0000
#define LAPIC_VERSION_LVTSHIFT		0x10
#define LAPIC_TPR           0x0080 /* Task Priority Register */
#define LAPIC_EOI           0x00B0 /* End of Interrupt */
#define LAPIC_SIV           0x00F0 /* Spurious Interrupt Vector */
#define LAPIC_SIV_ENABLE            0x100

#define LAPIC_ESR           0x0280 /* Error Status Register */
#define LAPIC_LVT_CMCI      0x02F0 /* LVT CMCI */

#define LAPIC_ICR_LO        0x0300 /* Interrupt Command Register */
#define LAPIC_ICR_HI        0x0310 /* Interrupt Command Register */
#define LAPIC_ICR_FIXED			0x0000 /* Delivery Mode */
#define LAPIC_ICR_NMI			0x0400
#define LAPIC_ICR_INIT			0x0500
#define LAPIC_ICR_STARTUP		0x0600
#define LAPIC_ICR_ASSERT		0x4000
#define LAPIC_ICR_TRIG			0x8000
#define LAPIC_ICR_SELF			0x00080000 /* Destination */
#define LAPIC_ICR_INCSELF		0x00080000
#define LAPIC_ICR_EXCSELF		0x000C0000
#define LAPIC_ICR_DELIVERY_PENDING	0x1000 /* Delivery Pending */

#define LAPIC_LVT_TIMER     0x0320 /* LVT Timer */
#define LAPIC_LVT_TIMER_ONESHOT     0x00000000
#define LAPIC_LVT_TIMER_PERIODIC    0x00020000
#define LAPIC_LVT_TIMER_TSCDEADLINE 0x00040000
#define LAPIC_LVT_THERMAL   0x0330 /* LVT Thermal Sensor */
#define LAPIC_LVT_PMCR      0x0340 /* LVT Performance Monitoring Counter */
#define LAPIC_LVT_LINT0     0x0350 /* LVT LINT0 */
#define LAPIC_LVT_LINT1     0x0360 /* LVT LINT1 */
#define LAPIC_LVT_ERROR     0x0370 /* LVT Error */
#define LAPIC_LVT_FLAG_MASKED	    0x00010000 /* Masked */
#define LAPIC_LVT_FLAG_NMI	    0x00000400 /* NMI */
#define LAPIC_LVT_FLAG_EXTINT	    0x00000700 /* ExtINT */

#define LAPIC_TICR          0x0380 /* Timer Initial Count Register */
#define LAPIC_TCCR          0x0390 /* Timer Currnet Count Register */
#define LAPIC_TDCR          0x03E0 /* Time Divide Configuration Register */
#define LAPIC_TDCR_X1               0x000B /* Divide counts by 1 */

bool lapicInitialized = false;

static uint32_t *
LAPIC_GetBase()
{
    uint64_t base = rdmsr(IA32_APIC_BASE_MSR) & 0xFFFFFFFFFFFFF000ULL;

    return (uint32_t *)DMPA2VA(base);
}

uint32_t
LAPIC_Read(uint16_t reg)
{
    uint32_t volatile *lapic = (uint32_t volatile *) LAPIC_GetBase();

    return lapic[reg >> 2];
}

void
LAPIC_Write(uint16_t reg, uint32_t val)
{
    uint32_t volatile *lapic = (uint32_t volatile *)LAPIC_GetBase();

    lapic[reg >> 2] = val;
    lapic[LAPIC_ID >> 2];
}

uint32_t
LAPIC_CPU()
{
    if (lapicInitialized)
	return LAPIC_Read(LAPIC_ID) >> 24;
    else
	return 0;
}

void
LAPIC_SendEOI()
{
    LAPIC_Write(LAPIC_EOI, 0);
}

void
LAPIC_Periodic(uint64_t rate)
{
    LAPIC_Write(LAPIC_TDCR, LAPIC_TDCR_X1);
    LAPIC_Write(LAPIC_LVT_TIMER, LAPIC_LVT_TIMER_PERIODIC | T_IRQ_TIMER);
    LAPIC_Write(LAPIC_TICR, rate);
}

__no_ubsan // cmosStartup is unaligned on purpose
void
LAPIC_StartAP(uint8_t apicid, uint32_t addr)
{
    // Setup CMOS stuff
    outb(0x70, 0x0F);
    outb(0x71, 0x0A);
    uint16_t *cmosStartup = (uint16_t *)DMPA2VA(0x467);
    cmosStartup[0] = 0;
    cmosStartup[1] = addr >> 4;

    // Send INIT
    LAPIC_Write(LAPIC_ICR_HI, apicid << 24);
    LAPIC_Write(LAPIC_ICR_LO, LAPIC_ICR_INIT | LAPIC_ICR_TRIG | LAPIC_ICR_ASSERT);
    // XXX: Delay
    LAPIC_Write(LAPIC_ICR_HI, apicid << 24);
    LAPIC_Write(LAPIC_ICR_LO, LAPIC_ICR_INIT | LAPIC_ICR_TRIG);
    // XXX: Delay

    // Send STARTUP
    LAPIC_Write(LAPIC_ICR_HI, apicid << 24);
    LAPIC_Write(LAPIC_ICR_LO, LAPIC_ICR_STARTUP | (addr >> 12));
    // XXX: Delay
    LAPIC_Write(LAPIC_ICR_HI, apicid << 24);
    LAPIC_Write(LAPIC_ICR_LO, LAPIC_ICR_STARTUP | (addr >> 12));
    // XXX: Delay
}

int
LAPIC_Broadcast(int vector)
{
    int i = 0;
    LAPIC_Write(LAPIC_ICR_LO, LAPIC_ICR_EXCSELF | vector);

    while ((LAPIC_Read(LAPIC_ICR_LO) & LAPIC_ICR_DELIVERY_PENDING) != 0) {
	pause();

	if (i > 1000000) {
	    kprintf("IPI not delivered?\n");
	    return -1;
	}
    }

    return 0;
}

int
LAPIC_BroadcastNMI(int vector)
{
    int i = 0;
    LAPIC_Write(LAPIC_ICR_LO, LAPIC_ICR_EXCSELF | LAPIC_ICR_NMI | vector);

    while ((LAPIC_Read(LAPIC_ICR_LO) & LAPIC_ICR_DELIVERY_PENDING) != 0) {
	pause();

	if (i > 1000000) {
	    kprintf("IPI not delivered?\n");
	    return -1;
	}
    }

    return 0;
}

void
LAPIC_Init()
{
    uint32_t version;
    uint32_t lvts;
    uint32_t edx;
    uint64_t base;

    cpuid(1, NULL, NULL, NULL, &edx);
    if ((edx & CPUID_FLAG_APIC) == 0)
        Panic("APIC is required!\n");

    // Disable ATPIC
    if (LAPIC_CPU() == 0) {
	outb(0xA1, 0xFF);
	outb(0x21, 0xFF);
    }

    // Enable LAPIC
    base = rdmsr(IA32_APIC_BASE_MSR);
    wrmsr(IA32_APIC_BASE_MSR, base | IA32_APIC_BASE_MSR_ENABLE);

    // Convert to Direct Map Address
    base = DMPA2VA(base);

    lapicInitialized = true;

    kprintf("LAPIC: CPU %d found at 0x%016llx\n", LAPIC_CPU(), base);

    version = LAPIC_Read(LAPIC_VERSION);
    lvts = (version & LAPIC_VERSION_LVTMASK) >> LAPIC_VERSION_LVTSHIFT;

    // Enable interrupts
    LAPIC_Write(LAPIC_SIV, LAPIC_SIV_ENABLE | T_IRQ_SPURIOUS);

    // Error Interrupt
    LAPIC_Write(LAPIC_LVT_ERROR, T_IRQ_ERROR);

    // Setup LINT0/1
    if (LAPIC_CPU() == 0) {
	LAPIC_Write(LAPIC_LVT_LINT0, LAPIC_LVT_FLAG_EXTINT);
	LAPIC_Write(LAPIC_LVT_LINT1, LAPIC_LVT_FLAG_NMI);
    } else {
	LAPIC_Write(LAPIC_LVT_LINT0, LAPIC_LVT_FLAG_MASKED);
	LAPIC_Write(LAPIC_LVT_LINT1, LAPIC_LVT_FLAG_MASKED);
    }

    // Performance Counter Interrupt
    if (lvts >= 4) {
	LAPIC_Write(LAPIC_LVT_PMCR, LAPIC_LVT_FLAG_MASKED);
    }

    // Thermal Interrupt
    if (lvts >= 5) {
	LAPIC_Write(LAPIC_LVT_THERMAL, T_IRQ_THERMAL);
    }

    // Machine Check Interrupt
    if (lvts >= 6) {
	LAPIC_Write(LAPIC_LVT_CMCI, LAPIC_LVT_FLAG_MASKED);
    }

    LAPIC_Periodic(10000000); // XXX: 100 Hz (changes must update trap.c as well)

    // Clear any remaining errors
    LAPIC_Write(LAPIC_ESR, 0);
    LAPIC_Write(LAPIC_ESR, 0);

    LAPIC_Write(LAPIC_ICR_HI, 0);
    LAPIC_Write(LAPIC_ICR_LO, LAPIC_ICR_INCSELF | LAPIC_ICR_INIT | LAPIC_ICR_TRIG);
    while (LAPIC_Read(LAPIC_ICR_LO) & LAPIC_ICR_DELIVERY_PENDING)
    {
	// XXX: Timeout
    }

    LAPIC_SendEOI();

    LAPIC_Write(LAPIC_TPR, 0);
}

static void
Debug_LAPIC(int argc, const char *argv[])
{
    uint32_t version = LAPIC_Read(LAPIC_VERSION);
    uint32_t lvts = (version & LAPIC_VERSION_LVTMASK) >> LAPIC_VERSION_LVTSHIFT;

    kprintf("LAPIC %d\n", LAPIC_CPU());
    kprintf("VERSION:    %08x\n", LAPIC_Read(LAPIC_VERSION));
    kprintf("ESR:        %08x\n", LAPIC_Read(LAPIC_ESR));
    kprintf("ICRLO:      %08x\n", LAPIC_Read(LAPIC_ICR_LO));
    kprintf("ICRHI:      %08x\n", LAPIC_Read(LAPIC_ICR_HI));
    kprintf("SIV:        %08x\n", LAPIC_Read(LAPIC_SIV));
    kprintf("ERROR:      %08x\n", LAPIC_Read(LAPIC_LVT_ERROR));
    if (lvts >= 5) {
	kprintf("THERMAL:    %08x\n", LAPIC_Read(LAPIC_LVT_THERMAL));
    }
    kprintf("LINT0:      %08x\n", LAPIC_Read(LAPIC_LVT_LINT0));
    kprintf("LINT1:      %08x\n", LAPIC_Read(LAPIC_LVT_LINT1));
    if (lvts >= 4) {
	kprintf("PMCR:       %08x\n", LAPIC_Read(LAPIC_LVT_PMCR));
    }
    if (lvts >= 6) {
	kprintf("CMCI:       %08x\n", LAPIC_Read(LAPIC_LVT_CMCI));
    }
}

REGISTER_DBGCMD(lapic, "LAPIC Status", Debug_LAPIC);

