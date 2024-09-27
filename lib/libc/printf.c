/*
 * Copyright (c) 2006-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

// static unsigned long getuint(va_list *ap, int lflag)
#define getuint(ap, lflag) \
    (lflag == 8) ? va_arg(ap, uint64_t) : \
    (lflag == 4) ? va_arg(ap, uint32_t) : \
    (lflag == 2) ? va_arg(ap, uint32_t) : \
    (lflag == 1) ? va_arg(ap, uint32_t) : 0

// static long getint(va_list *ap, int lflag)
#define getint(ap, lflag) \
    (lflag == 8) ? va_arg(ap, int64_t) : \
    (lflag == 4) ? va_arg(ap, int32_t) : \
    (lflag == 2) ? va_arg(ap, int32_t) : \
    (lflag == 1) ? va_arg(ap, int32_t) : 0

static const char *numberstring_lower = "0123456789abcdef";
static const char *numberstring_upper = "0123456789ABCDEF";

static void printnum(void (*func)(int, void*),void *handle,
                     uint64_t num,int base,int width,int padc)
{
    char buf[64];
    char *p = buf;
    int spaces;
    if (base < 0)
    {
        base = -base;
        do {
            *p = numberstring_upper[num % base];
            p++;
        } while (num /= base);
    } else {
        do {
            *p = numberstring_lower[num % base];
            p++;
        } while (num /= base);
    }

    // Print spacers (pre-number)
    spaces = width - (p - buf);
    if (padc == ' ' || padc == '0') {
	while (spaces > 0)
	{
	    func(padc, handle);
	    spaces--;
	}
    }

    // Print Number
    while (p != buf) {
        p--;
        func((int)*p, handle);
    }

    // Print spacers (post-number)
    if (padc == '-') {
	while (spaces > 0)
	{
	    func(' ', handle);
	    spaces--;
	}
    }
}

static int kvprintf(char const *fmt, void (*func)(int,void *), void *handle, va_list ap)
{
    const char *p;
    int ch;
    uint64_t unum;
    int64_t num;
    int lflag, width, padc;

    while (1) {
        while ((ch = *(unsigned char *)fmt++) != '%') {
            if (ch == '\0') return -1;
            func(ch, handle);
        }

        width = -1;
        lflag = 4;
        padc = ' ';
again:
        switch (ch = *(unsigned char *)fmt++) {
            case '-':
                padc = '-';
                goto again;
            case '0':
                padc = '0';
                goto again;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                width = 0;
                while (1) {
                    width = 10 * width + ch - '0';
                    ch = *fmt;
                    if (ch < '0' || ch > '9')
                    {
                        break;
                    }
                    fmt++;
                    if (ch == '\0')
                    {
                        fmt--;
                        goto again;
                    }
                }
                goto again;
            case 'c':
                func(va_arg(ap, int) & 0xff, handle);
                break;
            case 's': {
		int spaces;

		p = va_arg(ap, char *);
		if (p == NULL) {
		    func('*', handle);
		    func('N', handle);
		    func('U', handle);
		    func('L', handle);
		    func('L', handle);
		    func('*', handle);
		    break;
		}
		spaces = width - strlen(p);

		if (padc == ' ') {
		    while (spaces > 0)
		    {
			func(' ', handle);
			spaces--;
		    }
		}

		while (*p != '\0')
		{
		    func(*p++, handle);
		}

		if (padc == '-') {
		    while (spaces > 0)
		    {
			func(' ', handle);
			spaces--;
		    }
		}
		break;
            }
            case 'd':
                num = getint(ap, lflag);
                if (num < 0) {
                    func('-', handle);
                    unum = -num;
                } else {
                    unum = num;
                }
                printnum(func, handle, unum, 10, width, padc);
                break;
            case 'u':
                unum = getuint(ap, lflag);
                printnum(func, handle, unum, 10, width, padc);
                break;
            case 'o':
                unum = getuint(ap, lflag);
                printnum(func, handle, unum, 8, width, padc);
                break;
            case 'p':
                unum = (unsigned long)va_arg(ap, void *);
                printnum(func, handle, unum, 8, width, padc);
                break;
            case 'x':
                unum = getuint(ap, lflag);
                printnum(func, handle, unum, 16, width, padc);
                break;
            case 'X':
                unum = getuint(ap, lflag);
                printnum(func, handle, unum, -16, width, padc);
                break;
            case 'l':
                lflag = 8;
                goto again;
            case '%':
            default: // Print Literally
                func(ch, handle);
                break;
        }
    }
    return 0;
}

static void fileputc(int c, void* handle)
{
    fputc(c, (FILE *)handle);
}

int printf(const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = kvprintf(fmt, fileputc, (void *)stdout, ap);
    va_end(ap);

    return ret;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = kvprintf(fmt, fileputc, stream, ap);
    va_end(ap);

    return ret;
}

typedef struct StrState {
    char	*buf;
    char	*cur;
    size_t	maxlen;
} StrState;

static void strputc(int c, void *handle)
{
    StrState *state = (StrState *)handle;

    if ((state->maxlen != -1) &&
	(state->cur - state->buf >= state->maxlen)) {
	state->cur[0] = '\0';
	return;
    }

    state->cur[0] = c;
    state->cur++;
}

int sprintf(char *str, const char *fmt, ...)
{
    int ret;
    va_list ap;
    StrState state;

    state.buf = str;
    state.cur = str;
    state.maxlen = -1;

    va_start(ap, fmt);
    ret = kvprintf(fmt, strputc, &state, ap);
    va_end(ap);

    state.cur[0] = '\0';

    return ret;
}

int snprintf(char *str, size_t n, const char *fmt, ...)
{
    int ret;
    va_list ap;
    StrState state;

    state.buf = str;
    state.cur = str;
    state.maxlen = n;

    va_start(ap, fmt);
    ret = kvprintf(fmt, strputc, &state, ap);
    va_end(ap);

    state.cur[0] = '\0';

    return ret;
}

