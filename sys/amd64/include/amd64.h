/*
 * Copyright (c) 2013-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __AMD64_H__
#define __AMD64_H__

#include <sys/cdefs.h>

/*
 * Page Tables
 */

#define PGNUMMASK	0xFFFFFFFFFFFFF000ULL

#define PGIDXSHIFT	9
#define PGIDXMASK       (512 - 1)

#define PGSHIFT         12
#define PGSIZE          (1 << PGSHIFT)
#define PGMASK          (PGSIZE - 1)

#define LARGE_PGSHIFT   21
#define LARGE_PGSIZE    (1 << LARGE_PGSHIFT)
#define LARGE_PGMASK    (LARGE_PGSIZE - 1)

#define HUGE_PGSHIFT    30
#define HUGE_PGSIZE     (1 << HUGE_PGSHIFT)
#define HUGE_PGMASK     (HUGE_PGSIZE - 1)

#define ROUNDUP_PGSIZE(x)   (((x) + LARGE_PGSIZE - 1) & ~LARGE_PGMASK)
#define ROUNDDOWN_PGSIZE(x) ((x) & ~LARGE_PGMASK)

#define PTE_P   0x0001  /* Present */
#define PTE_W   0x0002  /* Writeable */
#define PTE_U   0x0004  /* User */
#define PTE_PWT 0x0008  /* Write Through */
#define PTE_PCD 0x0010  /* Cache Disable */
#define PTE_A   0x0020  /* Accessed */
#define PTE_D   0x0040  /* Dirty */
#define PTE_PS  0x0080  /* Page Size */
#define PTE_G   0x0100  /* Global */
#define PTE_OS1 0x0200  /* Available */
#define PTE_OS2 0x0400  /* Available */
#define PTE_OS3 0x0800  /* Available */
#define PTE_PAT 0x1000  /* Page Attribute Table */
#define PTE_NX  0x8000000000000000ULL /* No Execute */

#define PAGETABLE_ENTRIES   512

typedef uint64_t PageEntry;

typedef struct PageTable {
    PageEntry entries[PAGETABLE_ENTRIES];
} PageTable;

/*
 * Global Descriptor Table
 */

typedef struct PACKED PseudoDescriptor {
    uint16_t    lim;
    uint64_t    off;
} PseudoDescriptor;

#define SEG_G
#define SEG_DB
#define SEG_L
#define SEG_P
#define SEG_DPL_SHIFT 45
#define SEG_S

#define SEG_CS (0xE << 40)
#define SEG_DS (0x2 << 40)

#define SEG_TSA (0x9 << 40)
#define SEG_TSB (0xB << 40)

#define SEL_KCS 0x08
#define SEL_KDS 0x10
#define SEL_TSS 0x20
#define SEL_UCS 0x30
#define SEL_UDS 0x38

typedef uint64_t SegmentDescriptor;

/*
 * Interrupt Descriptor Table
 */

typedef struct PACKED InteruptGate64 {
    uint16_t    pc_low;
    uint16_t    cs;
    uint8_t     ist;
    uint8_t     type;
    uint16_t    pc_mid;
    uint32_t    pc_high;
    uint32_t    _unused1;
} InteruptGate64;

/*
 * Task State Segment
 */

typedef struct PACKED TaskStateSegment64 {
    uint32_t    _unused0;
    uint64_t    rsp0;
    uint64_t    rsp1;
    uint64_t    rsp2;
    uint64_t    _unused1;
    uint64_t    ist1;
    uint64_t    ist2;
    uint64_t    ist3;
    uint64_t    ist4;
    uint64_t    ist5;
    uint64_t    ist6;
    uint64_t    ist7;
    uint32_t    _unused2;
    uint32_t    _unused3;
    uint16_t    _unused4;
    uint16_t    iomap_offset;
} TaskStateSegment64;

/*
 * XSAVE Area
 */

typedef struct XSAVEArea
{
    uint16_t	fcw;
    uint16_t	fsw;
    uint8_t	ftw;
    uint8_t	_rsvd0;
    uint16_t	fop;
    uint64_t	fpuip;
    uint64_t	fpudp;
    uint32_t	mxcsr;
    uint32_t	mxcsr_mask;
    uint64_t	mmx[16];	// ST(n)/MMn (80-bits padded)
    uint64_t	xmm[32];	// XMM0-XMM15
} XSAVEArea;

/*
 * Control Registers
 */

#define CR0_PE      0x00000001 /* Protection Enabled */
#define CR0_MP      0x00000002 /* Monitor Coprocessor */
#define CR0_EM      0x00000004 /* Emulation */
#define CR0_TS      0x00000008 /* Task Switched */
#define CR0_ET      0x00000010 /* Extension Type */
#define CR0_NE      0x00000020 /* Numeric Error */
#define CR0_WP      0x00010000 /* Write Protect */
#define CR0_AM      0x00040000 /* Alignment Mask */
#define CR0_NW      0x20000000 /* Not Writethrough */
#define CR0_CD      0x40000000 /* Cache Disable */
#define CR0_PG      0x80000000 /* Paging */

#define CR4_VME     0x00000001 /* Virtual 8086 Mode Enable */
#define CR4_PVI     0x00000002 /* Protected-Mode Virtual Interupts */
#define CR4_TSD     0x00000004 /* Time Stamp Diable */
#define CR4_DE      0x00000008 /* Debugging Extensions */
#define CR4_PSE     0x00000010 /* Page Size Extensions */
#define CR4_PAE     0x00000020 /* Physical Address Extension */
#define CR4_MCE     0x00000040 /* Machine Check Enable */
#define CR4_PGE     0x00000080 /* Page Global Enable */
#define CR4_PCE     0x00000100 /* Performance Monitoring Counter Enable */
#define CR4_OSFXSR  0x00000200 /* OS FXSAVE/FXRSTOR Support */
#define CR4_OSXMMEXCPT 0x00000400 /* OS Unmasked Exception Support */
#define CR4_FSGSBASE 0x00010000 /* Enable FS/GS read/write Instructions */
#define CR4_OSXSAVE 0x00040000 /* XSAVE and Processor Extended States Enable */

#define RFLAGS_CF   0x00000001 /* Carry Flag */
#define RFLAGS_PF   0x00000004 /* Parity Flag */
#define RFLAGS_AF   0x00000010 /* Adjust Flag */
#define RFLAGS_ZF   0x00000040 /* Zero Flag */
#define RFLAGS_SF   0x00000080 /* Sign Flag */
#define RFLAGS_TF   0x00000100 /* Trap Flag */
#define RFLAGS_IF   0x00000200 /* Interrupt Enable Flag */
#define RFLAGS_DF   0x00000400 /* Direction Flag */
#define RFLAGS_OF   0x00000800 /* Overflow Flag */
// IOPL (bits 12-13)
#define RFLAGS_NT   0x00004000 /* Nested Task Flag */
#define RFLAGS_RF   0x00010000 /* Resume Flag */
#define RFLAGS_VM   0x00020000 /* Virtual 8086 Mode */
#define RFLAGS_AC   0x00040000 /* Alignment Check */
#define RFLAGS_VIF  0x00080000 /* Virtual Interrupt Flag */
#define RFLAGS_VIP  0x00100000 /* Virtual Interrupt Pending */
#define RFLAGS_ID   0x00200000 /* CPUID Supported */

/*
 * Debug Registers
 */

#define DR7_DR0L    0x00000001
#define DR7_DR0G    0x00000002
#define DR7_DR1L    0x00000004
#define DR7_DR1G    0x00000008
#define DR7_DR2L    0x00000010
#define DR7_DR2G    0x00000020
#define DR7_DR3L    0x00000040
#define DR7_DR3G    0x00000080

/*
 * MSRs
 */

#define MSR_EFER    0xC0000080

#define EFER_SCE    0x0001 /* Syscall Enable */
#define EFER_LME    0x0100 /* Long Mode Enable */
#define EFER_LMA    0x0400 /* Long Mode Active */
#define EFER_NXE    0x0800 /* Enable Execute Disable */
#define EFER_SVME   0x1000 /* SVM Enable (AMD) */
#define EFER_SLE    0x2000 /* Long Mode Segment Limit Enable (AMD) */
#define EFER_FFXSR  0x4000 /* Fast FXSAVE/FXRSTOR (AMD) */
#define EFER_TCE    0x8000 /* Translation Cache Extension (AMD) */

// SYSCALL/SYSRET
#define MSR_STAR    0xC0000081
#define MSR_LSTAR   0xC0000082
#define MSR_CSTAR   0xC0000083
#define MSR_SFMASK  0xC0000084

#include "amd64op.h"

#endif /* __AMD64_H__ */

