
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Castor Only
#include <syscall.h>
#include <sys/syscall.h>
#include <sys/dirent.h>

int
main(int argc, const char *argv[])
{
    int fd;
    int status;
    uintptr_t offset = 0;

    if (argc != 2) {
	fputs("Requires an argument\n", stdout);
	return 1;
    }

    fd = OSOpen(argv[1], 0);
    if (fd < 0) {
	fputs("Cannot open directory\n", stdout);
	return 1;
    }

    while (1) {
	struct dirent de;

	status = OSReadDir(fd, (char *)&de, sizeof(de), &offset);
	if (status == 0) {
	    break;
	}
	if (status < 0) {
	    printf("OSReadDir Error: %x\n", -status);
	    return 1;
	}

	printf("%s\n", de.d_name);
    }

    return 0;
}

