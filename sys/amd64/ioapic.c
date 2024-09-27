/*
 * IOAPIC
 */

#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/cdefs.h>

#include <machine/amd64.h>
#include <machine/pmap.h>
#include <machine/trap.h>

#define IOAPICBASE  0xFEC00000

#define IOAPICID    0x00 /* IOAPIC ID */
#define IOAPICVER   0x01 /* IOAPIC Version */
#define IOAPICARB   0x02 /* IOAPIC Arbitration ID */
#define IOREDTBL0   0x10
#define IOREDTBL23  0x3E

#define IOREDTBL_LEN    24

#define IOREDTBL_MASK       0x00010000
#define IOREDTBL_LOGICAL    0x00000800

uint32_t
IOAPIC_Read(uint32_t reg)
{
    uint32_t volatile *addr = (uint32_t volatile *)DMPA2VA(IOAPICBASE);
    uint32_t volatile *cmd = (uint32_t volatile *)DMPA2VA(IOAPICBASE + 0x10);

    ASSERT(reg <= 0xFF);

    *addr = reg;
    return *cmd;
}

void
IOAPIC_Write(uint32_t reg, uint32_t val)
{
    uint32_t volatile *addr = (uint32_t volatile *)DMPA2VA(IOAPICBASE);
    uint32_t volatile *cmd = (uint32_t volatile *)DMPA2VA(IOAPICBASE + 0x10);

    ASSERT(reg <= 0xFF);

    *addr = reg;
    *cmd = val;
}

void
IOAPIC_Init()
{
    int i;
    uint32_t id = (IOAPIC_Read(IOAPICID) >> 24) & 0x0F;
    uint32_t maxInts = (IOAPIC_Read(IOAPICVER) >> 16) & 0xFF;

    kprintf("IOAPIC: ID:%d Max Interrupts: %d\n", id, maxInts);

    for (i = 0; i < IOREDTBL_LEN; i++)
    {
        IOAPIC_Write(IOREDTBL0 + 2*i, IOREDTBL_MASK | (T_IRQ_BASE + i));
        IOAPIC_Write(IOREDTBL0 + 2*i + 1, 0);
    }
}

void
IOAPIC_Enable(int irq)
{
    uint32_t val = IOAPIC_Read(IOREDTBL0 + 2*irq);
    IOAPIC_Write(IOREDTBL0 + 2*irq, val & ~IOREDTBL_MASK);
}

void
IOAPIC_Disable(int irq)
{
    uint32_t val = IOAPIC_Read(IOREDTBL0 + 2*irq);
    IOAPIC_Write(IOREDTBL0 + 2*irq, val | IOREDTBL_MASK);
}

static void
Debug_IOAPIC(int argc, const char *argv[])
{
    int i;

    kprintf("IOAPIC ID:      %08x\n", IOAPIC_Read(IOAPICID));
    kprintf("IOAPIC VERSION: %08x\n", IOAPIC_Read(IOAPICVER));

    for (i = 0; i < IOREDTBL_LEN; i++)
    {
	uint32_t irqInfo = IOAPIC_Read(IOREDTBL0 + 2*i);
	uint32_t cpuInfo = IOAPIC_Read(IOREDTBL0 + 2*i + 1);
	kprintf("%02x: %08x %08x\n", i, irqInfo, cpuInfo);
    }
}

REGISTER_DBGCMD(ioapic, "IOAPIC Dump", Debug_IOAPIC);

