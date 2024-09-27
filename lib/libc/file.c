
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <syscall.h>

static FILE __stdin = { 1, STDIN_FILENO, 0 };
static FILE __stdout = { 1, STDOUT_FILENO, 0 };
static FILE __stderr = { 1, STDERR_FILENO, 0 };

FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;

static FILE fds[FOPEN_MAX] = {
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
};

FILE *
_alloc_file()
{
    int i;

    for (i = 0; i < FOPEN_MAX; i++) {
	if (fds[i].in_use == 0) {
	    fds[i].in_use = 1;
	    return &fds[i];
	}
    }

    // XXX: malloc

    return NULL;
}

void
_free_file(FILE *fh)
{
    fh->fd = 0;
    fh->offset = 0;
    fh->in_use = 0;

    // XXX: free
}

FILE *
fopen(const char *path, const char *mode)
{
    uint64_t fd;
    FILE *fh;
    // XXX: handle mode

    fd = OSOpen(path, 0);
    if (fd == 0)
	return NULL;

    fh = _alloc_file();
    fh->fd = fd;
    fh->offset = 0;

    // XXX: handle append

    return fh;
}

int
fclose(FILE *fh)
{
    int status = OSClose(fh->fd);

    _free_file(fh);

    return status;
}

int
feof(FILE *fh)
{
    errno = ENOSYS;
    return -1;
}

int
fflush(FILE *fh)
{
    errno = ENOSYS;
    return -1;
}

size_t
fread(void *buf, size_t size, size_t nmemb, FILE *fh)
{
    return OSRead(fh->fd, buf, 0, size * nmemb);
    // set errno
}

size_t
fwrite(const void *buf, size_t size, size_t nmemb, FILE *fh)
{
    return OSWrite(fh->fd, buf, 0, size * nmemb);
    // set errno
}

int
fputc(int ch, FILE *fh)
{
    if (fwrite(&ch, 1, 1, fh) == 1)
	return ch;
    return EOF;
}

int
fputs(const char *str, FILE *fh)
{
    int status = fwrite(str, strlen(str), 1, fh);
    if (status > 0)
	return status;
    // XXX: error handling
    return EOF;
}

int
puts(const char *str)
{
    int status, status2;
    status = fputs(str, stdout);
    if (status < 0)
	return EOF;

    status2 = fputc('\n', stdout);
    if (status2 < 0)
	return EOF;

    return status;
}

int
fgetc(FILE *fh)
{
    char ch;
    if (fread(&ch, 1, 1, fh) == 1)
	return ch;
    return EOF;
}

char *
fgets(char *str, int size, FILE *fh)
{
    int i;

    for (i = 0; i < (size - 1); i++) {
	int ch = fgetc(fh);
	if (ch == EOF)
	    return NULL;
	if (ch == '\b') {
	    if (i > 0)
		i -= 1;
	    i -= 1;
	    continue;
	}
	str[i] = (char)ch;
	if (ch == '\n') {
	    str[i + 1] = '\0';
	    return str;
	}
    }

    str[size - 1] = '\0';
    return str;
}

