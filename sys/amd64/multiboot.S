/*
 * Multiboot Entry
 */

#define STACK_SIZE 0x4000

#define MULTIBOOT_HEADER_MAGIC  0x1BADB002
#define MULTIBOOT_HEADER_FLAGS  0x00010003
//#define KERNEL_BASE 0x0000000000000000
#define KERNEL_BASE 0xFFFF800000000000
#define LOWMEM(_x) (_x - KERNEL_BASE)

.extern mb_entry

.text

/**
 * _start --
 *
 *	ELF entry point.
 */
.globl _start
_start: .code32
    jmp     multiboot_entry

/**
 * This header needs to be present near the start of the ELF binary that 
 * describes how the loader should load the operating system image into memory.
 */
.align  4
multiboot_header: .code32
.long   MULTIBOOT_HEADER_MAGIC
.long   MULTIBOOT_HEADER_FLAGS
.long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
.long   LOWMEM(multiboot_header)
.long   LOWMEM(_start)
.long   LOWMEM(_edata)
.long   LOWMEM(_end)
.long   LOWMEM(multiboot_entry)

// 
// Multiboot entry
// %eax: Magic (0x2BADB002)
// %ebx: multiboot info structure
//
multiboot_entry: .code32
    // Save multiboot magic
    movl    %eax, %edx

    // Setup the stack pointer
    movl    $LOWMEM(lmfarptr), %edi
    movw    $(0x7000 + 'A'), (0xB8000)
    movl    $LOWMEM(stack + STACK_SIZE), %esp

    // Reset EFLAGS to a known state
    movw    $(0x7000 + 'B'), (0xB8002)
    pushl   $0
    popf

    // Configure the CPU control register cr4
    movw    $(0x7000 + 'C'), (0xB8004)
    movl    %cr4, %eax
    orl     $0x0000006A0, %eax
    movl    %eax, %cr4

    // Load the 32-bit page table
    movw    $(0x7000 + 'D'), (0xB8006)
    movl    $LOWMEM(bootpgtbl1), %eax
    movl    %eax, %cr3

    // Enable 64-bit mode (long mode) and no-execute (NX) support
    movw    $(0x7000 + 'E'), (0xB8008)
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $0x0900, %eax
    wrmsr

    // Load the GDT that contains segment descriptors.
    movw    $(0x7000 + 'E'), (0xB800A)
    movl    $LOWMEM(bootgdtdesc), %eax
    lgdt    (%eax)

    // Configure cr0
    movw    $(0x7000 + 'F'), (0xB800C)
    movl    %cr0, %eax
    orl     $0x8005002B, %eax
    movl    %eax, %cr0

    // Long jump into 64-bit mode
    movw    $(0x7000 + '0'), (0xB800E)
    ljmp    *(%edi)

    // No we're playing with power
lmenter: .code64
    movw    $(0x7000 + '1'), (0xB8010)
    xorw    %ax, %ax
    movw    %ax, %ss
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    movq    $LOWMEM(stack + STACK_SIZE), %rax
    movq    $0xFFFF8000, %rcx
    shlq    $32, %rcx
    orq     %rcx, %rax
    movq    %rax, %rsp

    // Long jump into the high memory address where the kernel executes in
    movq    $LOWMEM(lmhighptr), %rdi
    jmp    *(%rdi)

lmhigh:
    movw    $(0x7000 + '2'), (0xB8012)

    movq    %rdx, %rdi // Magic
    movq    %rbx, %rsi // Multiboot info pointer

    call    MachineBoot_Entry

    # Print halt to the graphics memory if we return
    movw    $(0x5000 + 'H'), (0xB8098)
    movw    $(0x5000 + 'A'), (0xB809A)
    movw    $(0x5000 + 'L'), (0xB809C)
    movw    $(0x5000 + 'T'), (0xB809E)
loop:
    hlt
    jmp     loop

.p2align 4
lmfarptr:
.long   LOWMEM(lmenter)
.word   0x08

.p2align 4
lmhighptr:
.quad   lmhigh

.p2align 12
bootgdt:
.quad   0x0000000000000000 /* Null */
.quad   0x00AF9A000000FFFF /* Kernel CS */
.quad   0x00CF92000000FFFF /* Kernel DS */
.quad   0x0000000000000000
.quad   0x0000000000000000
.quad   0x0000000000000000
.quad   0x0000000000000000
.quad   0x0000000000000000

.p2align 4
.globl bootgdtdesc
bootgdtdesc:
.word   0x003F
.quad   LOWMEM(bootgdt)

// Boot stack
.globl stack
.comm   stack, STACK_SIZE, 16

