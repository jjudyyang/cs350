
#ifndef __STRING_H__
#define __STRING_H__

#include <stdint.h>

int memcmp(const void *b1, const void *b2, size_t len);
void *memcpy(void *dst, const void *src, size_t len);
void *memset(void *dst, int c, size_t len);

char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *to, const char *from);
size_t strlen(const char *str);
int strncmp(const char *s1, const char *s2, size_t len);
char *strncpy(char *to, const char *from, size_t len);

char *strcat(char *s, const char *append);
char *strncat(char *s, const char *append, size_t count);

char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **last);

#endif /* __STRING_H__ */

