
#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/irq.h>

#include "ioport.h"
#include "sercons.h"

#define COM1_BASE		0x3F8
#define COM2_BASE		0x2F8
#define COM3_BASE		0x3E8
#define COM4_BASE		0x2E8

#define COM1_IRQ		4
#define COM2_IRQ		3
#define COM3_IRQ		4
#define COM4_IRQ		3

// Inputs
#define UART_OFFSET_DATA	0   /* Data Register */
#define UART_OFFSET_IER		1   /* Interrupt Enable Register */
#define UART_OFFSET_IIR		2   /* Interrupt Identification & FIFO Control */
#define UART_OFFSET_LCR		3   /* Line Control Register */
#define UART_LCR_DLAB		0x80
#define UART_LCR_8N1		0x03
#define UART_OFFSET_MCR		4   /* Modem Control Register */
#define UART_OFFSET_LSR		5   /* Line Status Register */
#define UART_OFFSET_MSR		6   /* Modem Status Register */
#define UART_OFFSET_SR		7   /* Scratch Register */

#define UART_OFFSET_DIVLO	0   /* Divisors DLAB == 1 */
#define UART_OFFSET_DIVHI	1

static IRQHandler handler;
static uint16_t base;
static uint8_t irq;

void Serial_Init(void)
{
    base = COM1_BASE;
    irq = COM1_IRQ;

    // Disable interrupts
    outb(base + UART_OFFSET_IER, 0);

    // Enable DLAB
    outb(base + UART_OFFSET_LCR, UART_LCR_DLAB);
    outb(base + UART_OFFSET_DIVLO, 1); // 115200 Baud
    outb(base + UART_OFFSET_DIVLO, 0);
    outb(base + UART_OFFSET_LCR, UART_LCR_8N1);

    // Enable interrupts
    outb(base + UART_OFFSET_IIR, 0xC7);
    outb(base + UART_OFFSET_MCR, 0x0B);
}

void Serial_LateInit(void)
{
    handler.irq = irq;
    handler.cb = &Serial_Interrupt;
    handler.arg = NULL;

    IRQ_Register(irq, &handler);
}

void Serial_Interrupt(void *arg)
{
    kprintf("Serial interrupt!\n");
}

bool Serial_HasData()
{
    return (inb(base + UART_OFFSET_LSR) & 0x01) != 0;
}

char Serial_Getc()
{
    while ((inb(base + UART_OFFSET_LSR) & 0x01) == 0)
    {
	// Timeout!
    }
    return inb(base + UART_OFFSET_DATA);
}

void Serial_Putc(char ch)
{
    while ((inb(base + UART_OFFSET_LSR) & 0x20) == 0)
    {
	// Timeout!
    }
    outb(base + UART_OFFSET_DATA, ch);

    if (ch == '\b') {
	Serial_Putc(0x1B);
	Serial_Putc('[');
	Serial_Putc('P');
    }
}

void Serial_Puts(const char *str)
{
    const char *p = str;
    while (*p != '\0')
	Serial_Putc(*p++);
}

