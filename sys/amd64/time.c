
#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/ktime.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>

uint64_t
Time_GetTSC()
{
    return rdtsc();
}

static void
Debug_ReadTSC(int argc, const char *argv[])
{
    kprintf("RDTSC: %lld\n", Time_GetTSC());
}

REGISTER_DBGCMD(readtsc, "Print current timestamp", Debug_ReadTSC);

