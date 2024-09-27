
#ifndef __SYS_NIC_H__
#define __SYS_NIC_H__

#include <sys/queue.h>
#include <sys/mbuf.h>

typedef void (*NICCB)(int, void *);

typedef struct NIC NIC;
typedef struct NIC {
    void	*handle;					// Driver handle
    uint64_t	nicNo;						// NIC number
    uint8_t	mac[6];
    int		(*tx)(NIC *, MBuf *, NICCB, void *);		// TX
    int		(*rx)(NIC *, MBuf *, NICCB, void *);		// RX
    int		(*poll)();
    LIST_ENTRY(NIC) entries;
} NIC;

void NIC_AddNIC(NIC *nic);
void NIC_RemoveNIC(NIC *nic);
NIC *NIC_GetByID(uint64_t nicNo);
int NIC_GetMAC(NIC *nic, void *mac);
int NIC_TX(NIC *nic, MBuf *mbuf, NICCB cb, void *arg);
int NIC_RX(NIC *nic, MBuf *mbuf, NICCB cb, void *arg);
int NIC_Poll();

#endif /* __SYS_NIC_H__ */

