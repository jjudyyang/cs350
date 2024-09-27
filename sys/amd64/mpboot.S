
/**
 * Low level multiprocessor boot code
 */

#define KERNEL_BASE 0xFFFF800000000000
#define LOWMEM(_x) (_x - KERNEL_BASE)
#define MPLOWMEM(_x) (_x - KERNEL_BASE + 0x7000)

.extern bootpgtbl1
.extern stack
.extern Machine_InitAP

.text

/**
 * mpstart_begin --
 *
 *	Support processors start executing here in 16-bit compat mode.
 */
.code16
.globl mpstart_begin
mpstart_begin:
    // Disable interrupts
    cli

    // Initialize the segment registers to 0, which is an offset in 16-bit mode
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    // Load 32-bit GDT
    lgdt    (mpstartgdtdesc32 - mpstart_begin + 0x7000)

    movl    %cr0, %eax
    orl	    0x00000011, %eax
    movl    %eax, %cr0

    // Long jump to reload the code segment switching us into 32-bit mode
    ljmp    $0x08, $(mpstart_enter32 - mpstart_begin + 0x7000)

    nop
    nop

    // Now we're in 32-bit mode
.code32
mpstart_enter32:
    nop
    nop

    // Initialize segment registers
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    // Setup stack
    movw    %ax,%ss
    movl    $0x7000, %esp

    // Reset EFLAGS
    pushl   $0x0
    popf

    // Set CR4
    movl    %cr4, %eax
    orl     $0x0000006A0, %eax
    movl    %eax, %cr4

    // Temporary Page Table
    movl    $LOWMEM(bootpgtbl1), %eax
    movl    %eax, %cr3

    // Set EFER
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $0x0900, %eax
    wrmsr

    // Load 64-bit GDT
    movl    $LOWMEM(mpstartgdtdesc), %eax
    lgdt    (%eax)

    // Set CR0
    movl    %cr0, %eax
    // QEMU seems to set cr0 with garbage
    andl    $0x1FF5FFC0, %eax
    orl     $0x8005002B, %eax
    movl    %eax, %cr0

    // Long jump again to swithc us into 64-bit mode
    movl    $LOWMEM(mpstart_enter64ptr), %edi
    ljmp    *(%edi)

    nop
    nop

    // Now we're playing with power
.code64
mpstart_enter64:
    nop
    nop

    // Jump into the high memory address where the kernel should execute
    movq    $LOWMEM(mpstart_high64ptr), %rdi
    jmp	    *(%rdi)

    nop
    nop

mpstart_high64:
    nop
    nop

    // Load Initial CPU State
    movq    (0x6F00), %rax
    movq    (0x6F08), %rbx
    movq    %rax, %cr3
    movq    %rbx, %rsp

    call    Machine_InitAP

    // If we get here something went wrong so we write HALT to the display
1:
    movw    $(0x5000 + 'H'), (0xB8098)
    movw    $(0x5000 + 'A'), (0xB809A)
    movw    $(0x5000 + 'L'), (0xB809C)
    movw    $(0x5000 + 'T'), (0xB809E)
    jmp	    1b

.p2align 4
mpstart_enter64ptr:
.long	LOWMEM(mpstart_enter64)
.word	0x08

.p2align 4
mpstart_high64ptr:
.quad	mpstart_high64

/**
 * 32-bit GDT Table
 */
.p2align 12
mpstartgdt32:
.quad	0x0000000000000000 /* Null */
.quad	0x00CF9A000000FFFF /* Temporary CS */
.quad	0x00CF92000000FFFF /* Temporary DS */
.quad	0x0000000000000000
.quad	0x0000000000000000
.quad	0x0000000000000000
.quad	0x0000000000000000
.quad	0x0000000000000000

.p2align 4
mpstartgdtdesc32:
.word	0x003F
.long	LOWMEM(mpstartgdt32)

.p2align 12
mpstartgdt:
.quad	0x0000000000000000 /* Null */
.quad	0x00AF9C000000FFFF /* Kernel CS */
.quad	0x00CF92000000FFFF /* Kernel DS */
.quad	0x0000000000000000
.quad	0x0000000000000000
.quad	0x0000000000000000
.quad	0x0000000000000000
.quad	0x0000000000000000

.p2align 4
.quad	0x0
mpstartgdtdesc:
.word	0x003F
.quad	LOWMEM(mpstartgdt)

.globl mpstart_end
mpstart_end:

// Boot stack
//.globl	mpstack
//.comm	mpstack, STACK_SIZE

