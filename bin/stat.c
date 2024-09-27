
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

// Castor Only
#include <syscall.h>

int
main(int argc, const char *argv[])
{
    int status;
    struct stat sb;

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

    printf("  File: \"%s\"\n", argv[1]);
    printf("  Size: %d\n", sb.st_size);
    printf("Blocks: %d\n", sb.st_blocks);
    printf(" Inode: %d\n", sb.st_ino);

    return 0;
}

