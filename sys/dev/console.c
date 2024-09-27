
#include <stdbool.h>
#include <stdint.h>

#include <sys/spinlock.h>
#include <sys/kmem.h>
#include <sys/thread.h>

#include "console.h"
#include "x86/vgacons.h"
#include "x86/sercons.h"
#include "x86/debugcons.h"

Spinlock consoleLock;
Console consoles;

/*
 * Initialize console devices for debugging purposes.  At this point interrupts 
 * and device state is not initialized.
 */
void
Console_Init()
{
    VGA_Init();
    Serial_Init();
    DebugConsole_Init();

    Console_Puts("Castor Operating System\n");

    // XXXFILLMEIN
    Console_Puts(YOUR USERNAME HERE "\n");
    // For example, Console_Puts("yourname\n");

    Spinlock_Init(&consoleLock, "Console Lock", SPINLOCK_TYPE_NORMAL);

    Spinlock_Init(&consoles.keyLock, "Console Keyboard Lock", SPINLOCK_TYPE_NORMAL);
    consoles.nextKey = 0;
    consoles.lastKey = 0;
}

/*
 * Setup interrupts and input devices that may not be ready
 */
void
Console_LateInit()
{
    Serial_LateInit();
}

char
Console_Getc()
{
    while (1) {
	Spinlock_Lock(&consoles.keyLock);
	if (consoles.nextKey != consoles.lastKey) {
	    char key = consoles.keyBuf[consoles.nextKey];
	    consoles.nextKey = (consoles.nextKey + 1) % CONSOLE_KEYBUF_MAXLEN;
	    Spinlock_Unlock(&consoles.keyLock);
	    return key;
	}
	Spinlock_Unlock(&consoles.keyLock);

	// Support serial debugging if interrupts are disabled
	if (Serial_HasData()) {
	    char key = Serial_Getc();
	    switch (key) {
		case '\r':
		    return '\n';
		case 0x7f:
		    return '\b';
		default:
		    return key;
	    }
	}
    }
}

void
Console_EnqueueKey(char key)
{
    Spinlock_Lock(&consoles.keyLock);
    if (((consoles.lastKey + 1) % CONSOLE_KEYBUF_MAXLEN) == consoles.lastKey) {
	Spinlock_Unlock(&consoles.keyLock);
	return;
    }
    consoles.keyBuf[consoles.lastKey] = key;
    consoles.lastKey = (consoles.lastKey + 1) % CONSOLE_KEYBUF_MAXLEN;
    Spinlock_Unlock(&consoles.keyLock);
}

void
Console_Gets(char *str, size_t n)
{
    int i;

    for (i = 0; i < (n - 1); i++)
    {
	char ch = Console_Getc();
	if (ch == '\b') {
	    if (i > 0) {
		Console_Putc(ch);
		i--;
	    }
	    i--;
	    continue;
	}
	if (ch == '\n') {
	    Console_Putc('\n');
	    str[i] = '\0';
	    return;
	}
	if (ch == '\r') {
	    Console_Putc('\n');
	    str[i] = '\0';
	    return;
	}
	if (ch == '\t') {
	    Console_Putc('\t');
	    str[i] = '\t';
	    continue;
	}
	if (ch < 0x20)
	{
	    // Unprintable character
	    continue;
	}
	Console_Putc(ch);
	str[i] = ch;
    }

    str[i+1] = '\0';
}

void
Console_Putc(char ch)
{
    Spinlock_Lock(&consoleLock);
    VGA_Putc(ch);
    Serial_Putc(ch);
    DebugConsole_Putc(ch);
    Spinlock_Unlock(&consoleLock);
}

void
Console_Puts(const char *str)
{
    Spinlock_Lock(&consoleLock);
    VGA_Puts(str);
    Serial_Puts(str);
    DebugConsole_Puts(str);
    Spinlock_Unlock(&consoleLock);
}

int
Console_Read(Handle *handle, void *buf, uint64_t off, uint64_t len)
{
    uintptr_t b = (uintptr_t)buf;
    uint64_t i;

    for (i = 0; i < len; i++)
    {
	char c = Console_Getc();
	Console_Putc(c);
	Copy_Out(&c, b+i, 1);
    }

    return len;
}

int
Console_Write(Handle *handle, void *buf, uint64_t off, uint64_t len)
{
    int i;
    uintptr_t b = (uintptr_t)buf;
    char kbuf[512];
    uint64_t nbytes = 0;

    while (len > nbytes) {
	uint64_t chunksz = len > 512 ? 512 : len;
	Copy_In(b + nbytes, &kbuf, chunksz);
	nbytes += chunksz;

	for (i = 0; i < chunksz; i++)
	    Console_Putc(kbuf[i]);
    }

    return nbytes;
}

int
Console_Flush(Handle *handle)
{
    return 0;
}

int
Console_Close(Handle *handle)
{
    Handle_Free(handle);
    return 0;
}

Handle *
Console_OpenHandle()
{
    Handle *handle = Handle_Alloc();
    if (!handle)
	return NULL;

    handle->read = &Console_Read;
    handle->write = &Console_Write;
    handle->flush = &Console_Flush;
    handle->close = &Console_Close;

    return handle;
}

