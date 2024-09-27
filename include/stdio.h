
#ifndef __STDIO_H__
#define __STDIO_H__

#include <sys/types.h>
#include <sys/cdefs.h>

typedef struct FILE {
    int			in_use;
    uint64_t		fd;		/* Kernel File Descriptor */
    fpos_t		offset;
} FILE;

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

#define EOF		(-1)

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define FOPEN_MAX	16

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *fh);
int feof(FILE *fh);
int fflush(FILE *fh);
size_t fread(void *buf, size_t size, size_t nmemb, FILE *fh);
size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *fh);

int fputc(int ch, FILE *fh);
int fputs(const char *str, FILE *fh);
int fgetc(FILE *fh);
char *fgets(char *str, int size, FILE *fh);

int puts(const char *str);
#define getc(_fh) fgetc(_fh)

int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);
int sprintf(char *str, const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...) __printflike(3, 4);;

#endif /* __STDIO_H__ */

