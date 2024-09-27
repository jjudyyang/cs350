/*
 * Copyright (c) 2006-2007 Ali Mashtizadeh
 */

#include <stdbool.h>
#include <stdint.h>

#include <sys/cdefs.h>
#include <sys/kdebug.h>

#include <machine/amd64.h>
#include <machine/pmap.h>

#include "vgacons.h"

static short *VideoBuffer = (short *)(0x000B8000 + MEM_DIRECTMAP_BASE);
static int CurrentX = 0;
static int CurrentY = 0;
static int SizeX = 80;
static int SizeY = 25;
static unsigned char TextAttribute = 0x70; /* Grey on Black */

#define VGA_BASE 0x370

#define VGA_ATC_INDEX       0x00 + VGA_BASE
#define VGA_ATC_DATA        0x01 + VGA_BASE
#define VGA_SEQ_INDEX       0x02 + VGA_BASE
#define VGA_SEQ_DATA        0x05 + VGA_BASE
#define VGA_PAL_RADDR       0x07 + VGA_BASE
#define VGA_PAL_WADDR       0x08 + VGA_BASE
#define VGA_PAL_DATA        0x09 + VGA_BASE
#define VGA_GDC_INDEX       0x0E + VGA_BASE
#define VGA_GDC_DATA        0x0F + VGA_BASE

static uint8_t ModeBuffer[6];

void LockDisplay(void)
{
}

void UnlockDisplay(void)
{
}

void EnterFontMode(void)
{
    // Save VGA State
    outb(VGA_SEQ_INDEX,0x02);
    ModeBuffer[0] = inb(VGA_SEQ_DATA);

    outb(VGA_SEQ_INDEX,0x04);
    ModeBuffer[1] = inb(VGA_SEQ_DATA);

    outb(VGA_GDC_INDEX,0x04);
    ModeBuffer[2] = inb(VGA_GDC_DATA);

    outb(VGA_GDC_INDEX,0x05);
    ModeBuffer[3] = inb(VGA_GDC_DATA);

    outb(VGA_GDC_INDEX,0x06);
    ModeBuffer[4] = inb(VGA_GDC_DATA);

    outb(VGA_ATC_INDEX,0x10);
    ModeBuffer[5] = inb(VGA_ATC_DATA);

    // Setup Font Mode
}

void ExitFontMode(void)
{
    // Restore VGA State
}


void VGA_Init(void)
{
    int i = 0;

    for (i = 0;i < SizeX * SizeY;i++)
    {
        VideoBuffer[i] = (TextAttribute << 8) | ' ';
    }

    CurrentX = 0;
    CurrentY = 0;

    /*
     * At initialization the video memory is located at 0xB8000.
     * We will map the memory after memory management has been
     * initialized.
     */
}

void VGA_LateInit(void)
{
    // Map in video memory
    // Set VideoBuffer pointer
}

void VGA_ScrollDisplay(void)
{
    int i,j;
    for (i = 1; i < SizeY; i++)
    {
        for (j = 0; j < SizeX; j++)
        {
            VideoBuffer[(i-1)*SizeX+j] = VideoBuffer[i*SizeX+j];
        }
    }
    for (j = 0; j < SizeX; j++)
        VideoBuffer[(SizeY-1)*SizeX+j] = (TextAttribute << 8) | ' ';
    return;
}

void VGA_Putc(short c)
{
    c |= (TextAttribute << 8);
    switch (c & 0xFF)
    {
        case '\b':
	    if (CurrentX > 0) {
		CurrentX--;
		VideoBuffer[CurrentX + CurrentY * SizeX] = 0x20 | (TextAttribute << 8);
	    }
	    break;
        case '\n':
            if (CurrentY >= (SizeY - 1)) {
                VGA_ScrollDisplay();
            } else {
                CurrentY++;
            }
            CurrentX = 0;
            break;
        case '\r':
            break;
        case '\t':
            VGA_Putc(' ');
            VGA_Putc(' ');
            VGA_Putc(' ');
            VGA_Putc(' ');
            break;
        default:
            VideoBuffer[CurrentX + CurrentY * SizeX] = c;
            CurrentX++;
            if (CurrentX == SizeX)  {
                if (CurrentY >= (SizeY - 1)) {
                    VGA_ScrollDisplay();
                } else {
                    CurrentY++;
                }
                CurrentX = 0;
            }
            break;
    }
}

void VGA_Puts(const char *str)
{
    const char *p = str;
    while (*p != '\0')
        VGA_Putc(*p++);
}

void Panic(const char *str)
{
    VGA_Puts(str);
    __asm__("int3");
    while (1)
    {
        __asm__("hlt");
    }
}

