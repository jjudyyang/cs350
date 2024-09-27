
#ifndef __IRQ_H__
#define __IRQ_H__

#include <sys/queue.h>

typedef struct IRQHandler {
    int irq;
    void (*cb)(void*);
    void *arg;
    LIST_ENTRY(IRQHandler) link;
} IRQHandler;

void IRQ_Init();
void IRQ_Handler(int irq);
void IRQ_Register(int irq, struct IRQHandler *h);
void IRQ_Unregister(int irq, struct IRQHandler *h);

#endif /* __IRQ_H__ */

