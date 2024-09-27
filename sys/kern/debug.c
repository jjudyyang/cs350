/*
 * Copyright (c) 2012-2018 Ali Mashtizadeh
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>

#include "../dev/console.h"

#define DEBUG_MAX_LINE		128
#define DEBUG_MAX_ARGS		16

void
Debug_PrintHex(const char *data, size_t length, off_t off, size_t limit)
{
    const size_t row_size = 16;
    bool stop = false;

    size_t row;
    for (row = 0; !stop; row++) {
	size_t ix = row * row_size;
	if (ix >= limit || ix >= length)
	    return;

	kprintf("%08lx  ", (uintptr_t)data+ix);
	size_t col;
	for (col = 0; col < row_size; col++) { 
	    size_t ixc = ix + col;
	    if ((limit != 0 && ixc >= limit) || ixc >= length) {
		stop = true;
		for (; col < row_size; col++) {
		    kprintf("   ");
		}
		break;
	    }
	    ixc += off;

	    kprintf("%02X ", (unsigned char)data[ixc]);
	}
	kprintf("  |");

	for (col = 0; col < row_size; col++) { 
	    size_t ixc = ix + col;
	    if ((limit != 0 && ixc >= limit) || ixc >= length) {
		stop = true;
		for (; col < row_size; col++) {
		    kprintf(" ");
		}
		break;
	    }
	    ixc += off;

	    unsigned char c = (unsigned char)data[ixc];
	    if (c >= 0x20 && c < 0x7F)
		kprintf("%c", c);
	    else
		kprintf(".");
	}
	kprintf("|");
	kprintf("\n");
    }
}

uint64_t
Debug_GetValue(uintptr_t addr, int size, bool isSigned)
{
    switch (size)
    {
	case 1: {
	    uint8_t *val = (uint8_t *)addr;
	    if (isSigned && ((*val & 0x80) == 0x80)) {
		return (uint64_t)*val | 0xFFFFFFFFFFFFFF00ULL; 
	    }
	    return *val;
	}
	case 2: {
	    uint16_t *val = (uint16_t *)addr;
	    if (isSigned && ((*val & 0x8000) == 0x8000)) {
		return (uint64_t)*val | 0xFFFFFFFFFFFF0000ULL; 
	    }
	    return *val;
	}
	case 4: {
	    uint32_t *val = (uint32_t *)addr;
	    if (isSigned && ((*val & 0x80000000) == 0x80000000)) {
		return (uint64_t)*val | 0xFFFFFFFF00000000ULL; 
	    }
	    return *val;
	}
	case 8: {
	    uint64_t *val = (uint64_t *)addr;
	    return *val;
	}
	default: {
	    kprintf("Debug_GetValue: Unknown size parameter '%d'\n", size);
	    return (uint64_t)-1;
	}
    }
}

void
Debug_PrintSymbol(uintptr_t off, int strategy)
{
    kprintf("0x%llx", off);
}

uint64_t
Debug_StrToInt(const char *s)
{
    int i = 0;
    int base = 10;
    uint64_t val = 0;

    if (s[0] == '0' && s[1] == 'x')
    {
	base = 16;
	i = 2;
    }

    while (s[i] != '\0') {
	if (s[i] >= '0' && s[i] <= '9') {
	    val = val * base + (uint64_t)(s[i] - '0');
	} else if (s[i] >= 'a' && s[i] <= 'f') {
	    if (base != 16)
		kprintf("Not base 16!\n");
	    val = val * base + (uint64_t)(s[i] - 'a' + 10);
	} else if (s[i] >= 'A' && s[i] <= 'F') {
	    if (base != 16)
		kprintf("Not base 16!\n");
	    val = val * base + (uint64_t)(s[i] - 'A' + 10);
	} else {
	    kprintf("Not a number!\n");
	}
	i++;
    }

    return val;
}

uint64_t
Debug_SymbolToInt(const char *s)
{
    if (*s >= '0' || *s <= '9')
	return Debug_StrToInt(s);

    kprintf("Unknown symbol '%s'\n");
    return 0;
}

#define PHELP(_cmd, _msg) kprintf("%-16s %s\n", _cmd, _msg)

extern DebugCommand __kdbgcmd_start[];
extern DebugCommand __kdbgcmd_end[];

static void
Debug_Help(int argc, const char *argv[])
{
    int i;
    uintptr_t commands = (uintptr_t)&__kdbgcmd_end - (uintptr_t)&__kdbgcmd_start;
    commands /= sizeof(DebugCommand);
    DebugCommand *cmds = (DebugCommand *)&__kdbgcmd_start;

    kprintf("Commands:\n");
    for (i = 0; i < commands; i++)
    {
	kprintf("%-16s %s\n", cmds[i].name, cmds[i].description);
    }

    PHELP("continue", "Continue execution");
}

REGISTER_DBGCMD(help, "Display the list of commands", Debug_Help);

static void
Debug_Echo(int argc, const char *argv[])
{
    int i;

    for (i = 1; i < argc; i++)
    {
	kprintf("%s ", argv[i]);
    }
    kprintf("\n");
}

REGISTER_DBGCMD(echo, "Echo arguments", Debug_Echo);

static void
Debug_Dump(int argc, const char *argv[])
{
    uint64_t off, len;

    if (argc != 3)
    {
	kprintf("Usage: dump ADDR LENGTH\n");
	kprintf("Specify the address and length of the region you want to print a hex dump for!\n");
	return;
    }

    off = Debug_SymbolToInt(argv[1]);
    len = Debug_SymbolToInt(argv[2]);
    kprintf("Dump 0x%llx 0x%llx\n", off, len);
    Debug_PrintHex((const char *)off, len, 0, len);
}

REGISTER_DBGCMD(dump, "Dump a region of memory", Debug_Dump);

static void
Debug_Disasm(int argc, const char *argv[])
{
    uintptr_t off;
    int len = 1;

    if (argc != 2 && argc != 3) {
	kprintf("Usage: disasm ADDR [LENGTH]\n");
	kprintf("Specify the address of the instruction you want to disassemble!\n");
	return;
    }

    off = Debug_SymbolToInt(argv[1]);
    if (argc == 3)
	len = Debug_SymbolToInt(argv[2]);
    kprintf("Disassembly 0x%llx:\n", off);
    
    for (; len > 0; len--)
    {
	off = db_disasm(off, false);
    }
}

REGISTER_DBGCMD(disasm, "Disassemble", Debug_Disasm);

void
Debug_Prompt()
{
    int argc;
    char *argv[DEBUG_MAX_ARGS];
    char *nextArg, *last;
    char buf[DEBUG_MAX_LINE];

    kprintf("Entered Debugger!\n");

    /*
     * DebugCommand must be 128 bytes for the Section array to align properly
     */
    ASSERT(sizeof(DebugCommand) == 128);

    while (1) {
	kprintf("kdbg> ");

	// read input
	Console_Gets((char *)&buf, DEBUG_MAX_LINE);

	// parse input
	nextArg = strtok_r(buf, " \t\r\n", &last);
	for (argc = 0; argc < DEBUG_MAX_ARGS; argc++) {
	    if (nextArg == NULL)
		break;

	    argv[argc] = nextArg;
	    nextArg = strtok_r(NULL, " \t\r\n", &last);
	}

	if (strcmp(argv[0], "continue") == 0) {
	    return; // Continue
	} else {
	    // execute command
	    int i;
	    uintptr_t commands = (uintptr_t)&__kdbgcmd_end - (uintptr_t)&__kdbgcmd_start;
	    commands /= sizeof(DebugCommand);
	    DebugCommand *cmds = (DebugCommand *)&__kdbgcmd_start;
	    bool found = false;

	    for (i = 0; i < commands; i++)
	    {
		if (strcmp(argv[0], cmds[i].name) == 0)
		{
		    cmds[i].func(argc, (const char **)argv);
		    found = true;
		}
	    }

	    if (strcmp(argv[0], "") == 0)
		continue;

	    if (!found)
		kprintf("Unknown command '%s'\n", argv[0]);
	}
    }
}

