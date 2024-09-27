#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include <sys/syscall.h>

#define BLKSIZE (16384)
#define BLKS (65)

#define TMPFILE ("/LICENSE")

char inbuf[BLKSIZE];
char outbuf[BLKSIZE];

char
fillchar(int i)
{
	return ('a' + (i % ('z' - 'a')));
}

void
write(int fd, int i)
{
	size_t off = i * BLKSIZE;
	int ret;

	memset(inbuf, fillchar(i), sizeof(inbuf));

	ret = OSWrite(fd, inbuf, off, sizeof(inbuf));
	if (ret < 0) {
		printf("OSWrite: error at offset %d\n", off);
		OSExit(0);
	}

	if (ret == 0) {
		printf("OSWrite: no data written at offset %d\n", off);
		OSExit(0);
	}
}

void
writetest()
{
	int fd;
	int i;

	fd = OSOpen(TMPFILE, 0);
	if (fd < 0) {
		printf("OSOpen: error for file %s\n", TMPFILE);
		OSExit(0);
	}

	for (i = 0; i < BLKS; i++)
		write(fd, i);
}

void
read(int fd, int i)
{
	size_t off = i * BLKSIZE;
	int ret;

	ret = OSRead(fd, outbuf, off, sizeof(outbuf));
	if (ret < 0) {
		printf("OSRead: error at offset %d\n", off);
		OSExit(0);
	}

	if (ret == 0) {
		printf("OSWrite: no data read at offset %d\n", off);
		OSExit(0);
	}

	memset(inbuf, fillchar(i), sizeof(inbuf));

	if (memcmp(inbuf, outbuf, sizeof(inbuf)) != 0) {
		printf("read the wrong data back at offset %d\n", off);
		printf("Expected: %c %c %c %c\n", inbuf[0], inbuf[1], inbuf[2], inbuf[3]);
		printf("Got: %c %c %c %c\n", outbuf[0], outbuf[1], outbuf[2], outbuf[3]);
		OSExit(0);
	}
}

void
readtest()
{
	int fd;
	int i;

	fd = OSOpen(TMPFILE, 0);
	if (fd < 0) {
		printf("OSOpen: error for file %s\n", TMPFILE);
		OSExit(0);
	}

	for (i = 0; i < BLKS; i++)
		read(fd, i);
}


int
main(int argc, char **argv)
{
	writetest();
	readtest();

	printf("Success!\n");

	return (0);
}
