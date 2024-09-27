/*
 * IOAPIC Header
 */

#ifndef __IOAPIC_H__
#define __IOAPIC_H__

void IOAPIC_Init();
void IOAPIC_Enable(int irq);
void IOAPIC_Disable(int irq);

#endif /* __IOAPIC_H__ */

