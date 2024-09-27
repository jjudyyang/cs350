
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <syscall.h>

DIR *
opendir(const char *path)
{
    DIR *d = (DIR *)malloc(sizeof(DIR));

    d->fd = OSOpen(path, 0);
    if (d->fd < 0) {
	free(d);
	return NULL;
    }

    return d;
}

struct dirent *
readdir(DIR *d)
{
    int status = OSReadDir(d->fd, (char *)&d->de, sizeof(struct dirent), &d->offset);
    if (status == 0) {
	return NULL;
    }

    return &d->de;
}

void
rewinddir(DIR *d)
{
    d->offset = 0;
}

void
seekdir(DIR *d, long offset)
{
    d->offset = offset;
}

long
telldir(DIR *d)
{
    return d->offset;
}

int
closedir(DIR *d)
{
    OSClose(d->fd);
    free(d);

    return 0;
}

