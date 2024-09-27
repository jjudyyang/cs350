
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
    int i;
    int status, fd;
    struct stat sb;
    char buf[256];

    if (argc < 2) {
	fputs("Requires an argument\n", stdout);
	return 1;
    }

    if (argc > 2) {
	fputs("Too many arguments, expected one\n", stdout);
	return 1;
    }

    status = OSStat(argv[1], &sb);
    if (status != 0) {
	fputs("Cannot stat file\n", stdout);
	return 0;
    }
    fd = OSOpen(argv[1], 0);
    for (i = 0; i < sb.st_size; i += 256) {
	int len = (sb.st_size - i > 256) ? 256 : (sb.st_size - i);

	OSRead(fd, &buf, i, len);
	OSWrite(1, &buf, 0, len);
    }

    return 0;
}

