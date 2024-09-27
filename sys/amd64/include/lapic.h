/*
 * LAPIC Header
 */

#ifndef __LAPIC_H__
#define __LAPIC_H__

void LAPIC_Init();
uint32_t LAPIC_CPU();
void LAPIC_SendEOI();
void LAPIC_StartAP(uint8_t apicid, uint32_t addr);
int LAPIC_Broadcast(int vector);
int LAPIC_BroadcastNMI(int vector);
void LAPIC_Periodic(uint64_t rate);

#endif /* __LAPIC_H__ */

