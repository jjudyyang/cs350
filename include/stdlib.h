
#ifndef __STDLIB_H__
#define __STDLIB_H__

#include <sys/types.h>

#ifndef NULL
#define NULL    ((void *)0)
#endif

int atexit(void (*function)(void));
void exit(int status);
_Noreturn void abort(void);

void *calloc(size_t num, size_t sz);
void *malloc(size_t sz);
void free(void *buf);

int atoi(const char *nptr);

#endif /* __STDLIB_H__ */

