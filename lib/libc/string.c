/*
 * Copyright (c) 2006-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

char *
strchr(const char *s, int c)
{
    int i;
    for (i = 0; ; i++)
    {
	if (s[i] == c)
	    return (char *)s + i;
	if (s[i] == '\0')
	    return NULL;
    }
}

char *
strcpy(char *to, const char *from)
{
    char *save = to;

    for (; (*to = *from); ++from, ++to);

    return save;
}

char *
strncpy(char *to, const char *from, size_t length)
{
    char *save = to;

    for (; (*to = *from) != '\0' && length > 0; ++from, ++to, length--);

    *to = '\0';

    return save;
}

char *
strcat(char *s, const char *append)
{
    char *save = s;
 
    for (; *s; ++s);
    while ((*s++ = *append++));

    return save;
}


char *
strncat(char *dst, const char *src, size_t n)
{
    if (n != 0) {
	char *d = dst;
	const char *s = src;

	while (*d != 0)
	    d++;

	do {
	    if ((*d = *s++) == 0)
		break;

	    d++;
	} while (--n != 0);

	*d = 0;
    }

    return dst;
}

int
strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2++)
        if (*s1++ == 0)
            return 0;

    return (*(const uint8_t *)s1 - *(const uint8_t *)(s2 - 1));
}

int
strncmp(const char *s1, const char *s2, size_t len)
{
    if (len == 0)
	return 0;

    while (*s1 == *s2) {
	if (*s1 == 0)
	    return 0;

	s1++;
	s2++;

	len--;
	if (len == 0)
	    return 0;
    }

    return (*(const uint8_t *)s1 - *(const uint8_t *)s2);
}

size_t
strlen(const char *str)
{
    const char *s;

    for (s = str; *s; ++s);

    return (s - str);
}

char *
strtok_r(char *str, const char *delim, char **last)
{
    char *rval;

    if (str == NULL)
	str = *last;

    // Skip deliminaters in the front
    while ((str[0] != '\0') && strchr(delim, str[0])) {
	str++;
    }

    // We've reached the end of the string
    if (str[0] == '\0')
	return NULL;

    // Return first non-delim until the next
    rval = str;

    // Skip deliminaters in the front
    while ((str[0] != '\0') && !strchr(delim, str[0])) {
	str++;
    }

    if (str[0] != '\0') {
	str[0] = '\0';
	str++;
    }
    *last = str;

    return rval;
}

char *
strtok(char *str, const char *delim)
{
    static char *last;

    return strtok_r(str, delim, &last);
}

void *
memset(void *dst, int c, size_t length)
{
    uint8_t *p = (uint8_t *)dst;

    while (length-- != 0) {
        *p = c;
        p += 1;
    };

    return dst;
}

void *
memcpy(void *dst, const void *src, size_t length)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    while (length-- != 0) {
	*d = *s;
	d += 1;
	s += 1;
    };

    return dst;
}

int
memcmp(const void *b1, const void *b2, size_t length)
{
    int i;
    const char *c1 = (const char *)b1;
    const char *c2 = (const char *)b2;

    for (i = 0; i < length; i++)
    {
	if (*c1 != *c2)
	    return *c2 - *c1;
	c1++;
	c2++;
    }

    return 0;
}

