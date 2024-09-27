
#ifndef __MACHINE_MP_H__
#define __MACHINE_MP_H__

#define CPUSTATE_NOT_PRESENT	0
#define CPUSTATE_BOOTED		1
#define CPUSTATE_HALTED		2
#define CPUSTATE_MAX		2

void MP_Init();
void MP_InitAP();
void MP_SetState(int state);
int MP_GetCPUs();

/* Cross Calls */
typedef int (*CrossCallCB)(void *);
void MP_CrossCallTrap();
int MP_CrossCall(CrossCallCB cb, void *arg);

uint32_t LAPIC_CPU();
#define THISCPU	    LAPIC_CPU

#endif /* __MACHINE_MP__ */

