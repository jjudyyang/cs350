
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/irq.h>

#include <machine/trap.h>
#include <machine/ioapic.h>

LIST_HEAD(IRQHandlerList, IRQHandler);
struct IRQHandlerList handlers[T_IRQ_LEN];

void
IRQ_Init()
{
    int i;

    for (i = 0; i < T_IRQ_LEN; i++)
    {
	LIST_INIT(&handlers[i]);
    }
}

void
IRQ_Handler(int irq)
{
    struct IRQHandler *h;
    LIST_FOREACH(h, &handlers[irq], link)
    {
	h->cb(h->arg);
    }
}

void
IRQ_Register(int irq, struct IRQHandler *h)
{
    ASSERT(irq < T_IRQ_LEN);

    LIST_INSERT_HEAD(&handlers[irq], h, link);

    IOAPIC_Enable(irq);
}

void
IRQ_Unregister(int irq, struct IRQHandler *h)
{
    LIST_REMOVE(h, link);

    if (LIST_EMPTY(&handlers[irq]))
	IOAPIC_Disable(irq);
}

