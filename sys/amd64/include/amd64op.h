/*
 * Copyright (c) 2013-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __AMD64OP_H__
#define __AMD64OP_H__

static INLINE void enable_interrupts()
{
    asm volatile("sti");
}

static INLINE void disable_interrupts()
{
    asm volatile("cli");
}

static INLINE void hlt()
{
    asm volatile("hlt");
}

static INLINE void pause()
{
    asm volatile("pause");
}

static INLINE void breakpoint()
{
    asm volatile("int3");
}

static INLINE void icebp()
{
    asm volatile(".byte 0xf1");
}

static INLINE uint64_t rdtsc()
{
    uint32_t lo, hi;

    asm volatile("rdtsc"
	    : "=a" (lo), "=d" (hi));

    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static INLINE uint64_t rdtscp(uint32_t *procno)
{
    uint32_t lo, hi, proc;

    asm volatile("rdtsc"
	    : "=a" (lo), "=d" (hi), "=c" (proc));

    if (procno)
	*procno = proc;

    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static INLINE void lidt(PseudoDescriptor *idt)
{
    asm volatile("lidt (%0)"
        :
        : "r" (idt)
        : "memory");
}

static INLINE void lgdt(PseudoDescriptor *gdt)
{
    asm volatile("lgdt (%0)"
        :
        : "r" (gdt)
        : "memory");
}

static INLINE void ltr(uint16_t tss)
{
    asm volatile("ltr %0"
        :
        : "r" (tss));
}

static INLINE void cpuid(uint32_t info, uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    uint32_t a, b, c, d;

    asm volatile("cpuid"
        : "=a" (a), "=b" (b), "=c" (c), "=d" (d)
        : "a" (info));

    if (eax)
        *eax = a;
    if (ebx)
        *ebx = b;
    if (ecx)
        *ecx = c;
    if (edx)
        *edx = d;
}

static INLINE void wrmsr(uint32_t addr, uint64_t val)
{
    uint32_t eax = val & 0xFFFFFFFF;
    uint32_t edx = val >> 32;

    asm volatile("wrmsr"
        :
        : "a" (eax), "c" (addr), "d" (edx));
}

static INLINE uint64_t rdmsr(uint32_t addr)
{
    uint64_t eax, edx;

    asm volatile("rdmsr"
        : "=a" (eax), "=d" (edx)
        : "c" (addr));

    return edx << 32 | eax;
}

/*
 * Control Registers
 */

static INLINE uint64_t read_cr0()
{
    uint64_t val;

    asm volatile("movq %%cr0, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_cr0(uint64_t val)
{
    asm volatile("movq %0, %%cr0"
        :
        : "r" (val));
}

static INLINE uint64_t read_cr2()
{
    uint64_t val;

    asm volatile("movq %%cr2, %0"
        : "=r" (val));

    return val;
}

static INLINE uint64_t read_cr3()
{
    uint64_t val;

    asm volatile("movq %%cr3, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_cr3(uint64_t val)
{
    asm volatile("movq %0, %%cr3"
        :
        : "r" (val));
}

static INLINE uint64_t read_cr4()
{
    uint64_t val;

    asm volatile("movq %%cr4, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_cr4(uint64_t val)
{
    asm volatile("movq %0, %%cr4"
        :
        : "r" (val));
}

/*
 * Debug Registers
 */

static INLINE uint64_t read_dr0()
{
    uint64_t val;

    asm volatile("movq %%dr0, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_dr0(uint64_t val)
{
    asm volatile("movq %0, %%dr0"
        :
        : "r" (val));
}

static INLINE uint64_t read_dr1()
{
    uint64_t val;

    asm volatile("movq %%dr1, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_dr1(uint64_t val)
{
    asm volatile("movq %0, %%dr1"
        :
        : "r" (val));
}

static INLINE uint64_t read_dr2()
{
    uint64_t val;

    asm volatile("movq %%dr2, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_dr2(uint64_t val)
{
    asm volatile("movq %0, %%dr2"
        :
        : "r" (val));
}

static INLINE uint64_t read_dr3()
{
    uint64_t val;

    asm volatile("movq %%dr3, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_dr3(uint64_t val)
{
    asm volatile("movq %0, %%dr3"
        :
        : "r" (val));
}

static INLINE uint64_t read_dr6()
{
    uint64_t val;

    asm volatile("movq %%dr6, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_dr6(uint64_t val)
{
    asm volatile("movq %0, %%dr6"
        :
        : "r" (val));
}

static INLINE uint64_t read_dr7()
{
    uint64_t val;

    asm volatile("movq %%dr7, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_dr7(uint64_t val)
{
    asm volatile("movq %0, %%dr7"
        :
        : "r" (val));
}

/*
 * Segment Registers
 */

static INLINE uint16_t read_ds()
{
    uint16_t val;

    asm volatile("movw %%ds, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_ds(uint16_t val)
{
    asm volatile("movw %0, %%ds"
        :
        : "r" (val));
}

static INLINE uint16_t read_es()
{
    uint16_t val;

    asm volatile("movw %%es, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_es(uint16_t val)
{
    asm volatile("movw %0, %%es"
        :
        : "r" (val));
}

static INLINE uint16_t read_fs()
{
    uint16_t val;

    asm volatile("movw %%fs, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_fs(uint16_t val)
{
    asm volatile("movw %0, %%fs"
        :
        : "r" (val));
}

static INLINE uint16_t read_gs()
{
    uint16_t val;

    asm volatile("movw %%gs, %0"
        : "=r" (val));

    return val;
}

static INLINE void write_gs(uint16_t val)
{
    asm volatile("movw %0, %%gs"
        :
        : "r" (val));
}

/*
 * Floating Point
 */

static INLINE void clts()
{
    asm volatile("clts");
}

static INLINE void fxsave(struct XSAVEArea *xsa)
{
    asm volatile("fxsave %0"
	    : "=m" (*xsa)
	    :
	    : "memory");
}

// XXX: Need to fix AMD Bug
static INLINE void fxrstor(struct XSAVEArea *xsa)
{
    asm volatile("fxrstor %0"
	    :
	    : "m" (*xsa)
	    : "memory");
}

static INLINE void xsave(struct XSAVEArea *xsa, uint64_t mask)
{
    uint32_t lo = (uint32_t)mask;
    uint32_t hi = (uint32_t)(mask >> 32);

    asm volatile("xsave %0"
	    : "=m" (*xsa)
	    : "a" (lo), "d" (hi)
	    : "memory");
}

static INLINE void xsaveopt(struct XSAVEArea *xsa, uint64_t mask)
{
    uint32_t lo = (uint32_t)mask;
    uint32_t hi = (uint32_t)(mask >> 32);

    asm volatile("xsaveopt %0"
	    : "=m" (*xsa)
	    : "a" (lo), "d" (hi)
	    : "memory");
}

static INLINE void xrstor(struct XSAVEArea *xsa, uint64_t mask)
{
    uint32_t lo = (uint32_t)mask;
    uint32_t hi = (uint32_t)(mask >> 32);

    asm volatile("xrstor %0"
	    :
	    : "m" (*xsa), "a" (lo), "d" (hi)
	    : "memory");
}

/*
 * Port IO
 */

static INLINE void outb(uint16_t port, uint8_t data)
{
    asm volatile("outb %0, %1"
	:
	: "a" (data), "d" (port));
}

static INLINE void outw(uint16_t port, uint16_t data)
{
    asm volatile("outw %0, %1"
	:
	: "a" (data), "d" (port));
}

static INLINE void outl(uint16_t port, uint32_t data)
{
    asm volatile("outl %0, %1"
	:
	: "a" (data), "d" (port));
}

static INLINE uint8_t inb(uint16_t port)
{
    uint8_t data;

    asm volatile("inb %1, %0"
	: "=a" (data)
	:"d" (port));

    return data;
}

static INLINE uint16_t inw(uint16_t port)
{
    uint16_t data;

    asm volatile("inw %1, %0"
	: "=a" (data)
	:"d" (port));

    return data;
}

static INLINE uint32_t inl(uint16_t port)
{
    uint32_t data;

    asm volatile("inl %1, %0"
	: "=a" (data)
	:"d" (port));

    return data;
}

#endif /* __AMD64OP_H__ */

