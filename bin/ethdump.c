
#include <stdio.h>
#include <errno.h>
#include <syscall.h>
#include <sys/nic.h>

#include <net/ethernet.h>

static int nicNo = 1;
static char buf[4096];
static MBuf mbuf;

void
dumpPacket()
{
    struct ether_header *hdr = (struct ether_header *)&buf;
    char srcMac[18];
    char dstMac[18];

    sprintf(srcMac, "%02x:%02x:%02x:%02x:%02x:%02x",
	    hdr->ether_shost[0], hdr->ether_shost[1], hdr->ether_shost[2],
	    hdr->ether_shost[3], hdr->ether_shost[4], hdr->ether_shost[5]);
    sprintf(dstMac, "%02x:%02x:%02x:%02x:%02x:%02x",
	    hdr->ether_dhost[0], hdr->ether_dhost[1], hdr->ether_dhost[2],
	    hdr->ether_dhost[3], hdr->ether_dhost[4], hdr->ether_dhost[5]);

    printf("From %s to %s of type %04x\n", srcMac, dstMac, hdr->ether_type);
}

void
readPacket(NIC *nic)
{
    uint64_t status;

    mbuf.vaddr = (uint64_t)&buf;
    mbuf.maddr = 0;
    mbuf.len = 4096;
    mbuf.type = MBUF_TYPE_NULL;
    mbuf.flags = 0;
    mbuf.status = MBUF_STATUS_NULL;

    status = OSNICRecv(nicNo, &mbuf);
    if (status != 0) {
	printf("OSNICRecv failed!\n");
	return;
    }

    if (mbuf.status == MBUF_STATUS_FAILED) {
	printf("Failed to read packet!\n");
	return;
    }

    dumpPacket();
}

int
main(int argc, const char *argv[])
{
    uint64_t status;
    NIC nic;

    printf("Ethernet Dump Tool\n");

    status = OSNICStat(nicNo, &nic);
    if (status == ENOENT) {
	printf("nic%d not present\n", nicNo);
	return 1;
    }

    printf("Listening to nic%d\n", (int)nic.nicNo);

    while (1) {
	readPacket(&nic);
    }
}

