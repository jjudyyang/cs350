/*
 * Copyright (c) 2006-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdint.h>

static __inline__ unsigned char inb(unsigned short port)
{
    unsigned char retval;
    __asm__ __volatile__ ("inb %w1, %0\n\t"
    : "=a" (retval)
    : "d" (port));
    return retval;
}

static __inline__ unsigned short inw(unsigned short port)
{
    unsigned short retval;
    __asm__ __volatile__ ("inw %w1, %0\n\t"
    : "=a" (retval)
    : "d" (port));
    return retval;
}

static __inline__ unsigned int inl(int port)
{
    unsigned int retval;
    __asm__ __volatile__ ("inl %w1, %0\n\t"
    : "=a" (retval)
    : "d" (port));
    return retval;
}

static __inline__ void outb(int port, unsigned char val)
{
    __asm__ __volatile__ ("outb %0, %w1\n\t"
    :
    : "a" (val),
    "d" (port));
}

static __inline__ void outw(int port, unsigned short val)
{
    __asm__ __volatile__ ("outw %0, %w1\n\t"
    :
    : "a" (val),
    "d" (port));
}

static __inline__ void outl(int port, unsigned int val)
{
    __asm__ __volatile__ ("outl %0, %w1\n\t"
    :
    : "a" (val),
    "d" (port));
}

static __inline__ void insb(int port,void *buf,int cnt)
{
    __asm__ __volatile__ ("cld\n\trepne\n\tinsb\n\t"
    : "=D" (buf), "=c" (cnt)
    : "d" (port), "0" (buf), "1" (cnt) : "memory", "cc");
}

static __inline__ void insw(int port,void *buf,int cnt)
{
    __asm__ __volatile__ ("cld\n\trepne\n\tinsw\n\t"
    : "=D" (buf), "=c" (cnt)
    : "d" (port), "0" (buf), "1" (cnt) : "memory", "cc");
}

static __inline__ void insl(int port,void *buf,int cnt)
{
    __asm__ __volatile__ ("cld\n\trepne\n\tinsl\n\t"
    : "=D" (buf), "=c" (cnt)
    : "d" (port), "0" (buf), "1" (cnt) : "memory", "cc");
}

static __inline__ void outsb(int port,const void *buf,int cnt)
{
    __asm__ __volatile__ ("cld\n\trepne\n\toutsb\n\t"
    : "=S" (buf), "=c" (cnt)
    : "d" (port), "0" (buf), "1" (cnt) : "cc");
}

static __inline__ void outsw(int port,const void *buf,int cnt)
{
    __asm__ __volatile__ ("cld\n\trepne\n\toutsw\n\t"
    : "=S" (buf), "=c" (cnt)
    : "d" (port), "0" (buf), "1" (cnt) : "cc");
}

static __inline__ void outsl(int port,const void *buf,int cnt)
{
    __asm__ __volatile__ ("cld\n\trepne\n\toutsl\n\t"
    : "=S" (buf), "=c" (cnt)
    : "d" (port), "0" (buf), "1" (cnt) : "cc");
}
