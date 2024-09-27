
#include <stdio.h>
#include <errno.h>
#include <syscall.h>

int
main(int argc, const char *argv[])
{
    int i;
    uint64_t status;
    NIC nic;

    printf("Network Status\n");

    for (i = 0; i < 32; i++) {
	status = OSNICStat(i, &nic);
	if (status == ENOENT)
	    continue;

	printf("nic%d:\n", (int)nic.nicNo);
	printf("        ether %02x:%02x:%02x:%02x:%02x:%02x\n",
		nic.mac[0], nic.mac[1], nic.mac[2],
		nic.mac[3], nic.mac[4], nic.mac[5]);
    }

    return 0;
}

