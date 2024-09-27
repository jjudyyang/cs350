
#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/kconfig.h>
#include <sys/mp.h>
#include <sys/spinlock.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>

uint32_t lockLevel[MAX_CPUS];

void
Critical_Init()
{
    int c;

    for (c = 0; c < MAX_CPUS; c++)
    {
	lockLevel[c] = 0;
    }
}

void
Critical_Enter()
{
    disable_interrupts();
    lockLevel[CPU()]++;
}

void
Critical_Exit()
{
    lockLevel[CPU()]--;
    if (lockLevel[CPU()] == 0)
    {
	enable_interrupts();
    }
}

uint32_t
Critical_Level()
{
    return lockLevel[CPU()];
}

static void
Debug_Critical(int argc, const char *argv[])
{
    int c;

    for (c = 0; c < MAX_CPUS; c++) {
	kprintf("CPU%d: %u\n", c, lockLevel[c]);
    }
}

REGISTER_DBGCMD(critical, "Critical Enter/Exit Stats", Debug_Critical);

