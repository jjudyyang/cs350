
/*
 * Debug Console for QEmu compatible port 0xe9
 */

#include <stdint.h>

#include <sys/cdefs.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>

void DebugConsole_Init()
{
}

void DebugConsole_Putc(short c)
{
    outb(0xe9, (uint8_t)c);
}

void DebugConsole_Puts(const char *str)
{
    const char *p = str;
    while (*p != '\0')
        DebugConsole_Putc(*p++);
}

