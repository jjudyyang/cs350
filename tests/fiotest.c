
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Castor Only
#include <syscall.h>

int
main(int argc, const char *argv[])
{
    printf("FIO Test\n");
    uint64_t fd = OSOpen("/LICENSE", 0);

    for (int i = 0; i < 100; i++) {
	int status = OSWrite(fd, "123456789\n", i*10, 10);
	if (status < 0) {
	    printf("Error: %x\n", -status);
	    return 1;
	}
    }

    printf("Success!\n");

    return 0;
}

