
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <net/ethernet.h>

#include <syscall.h>
#include <sys/nic.h>

static int nicNo = 1;
static char buf[4096];
static MBuf mbuf;

void
writePacket(NIC *nic)
{
    uint64_t status;

    mbuf.vaddr = (uint64_t)&buf;
    mbuf.maddr = 0;
    mbuf.len = 64;
    mbuf.type = MBUF_TYPE_NULL;
    mbuf.flags = 0;
    mbuf.status = MBUF_STATUS_NULL;

    status = OSNICSend(nicNo, &mbuf);
    if (status != 0) {
	printf("OSNICSend failed!\n");
	return;
    }

    if (mbuf.status == MBUF_STATUS_FAILED) {
	printf("Failed to write packet!\n");
	return;
    }
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

    printf("Injecting packet on nic%d\n", (int)nic.nicNo);

    struct ether_header *hdr = (struct ether_header *)&buf;
    hdr->ether_dhost[0] = 0xFF;
    hdr->ether_dhost[1] = 0xFF;
    hdr->ether_dhost[2] = 0xFF;
    hdr->ether_dhost[3] = 0xFF;
    hdr->ether_dhost[4] = 0xFF;
    hdr->ether_dhost[5] = 0xFF;

    hdr->ether_shost[0] = 0x00;
    hdr->ether_shost[1] = 0x11;
    hdr->ether_shost[2] = 0x22;
    hdr->ether_shost[3] = 0x33;
    hdr->ether_shost[4] = 0x44;
    hdr->ether_shost[5] = 0x55;

    while (1) {
	writePacket(&nic);
	sleep(5);
    }
}

