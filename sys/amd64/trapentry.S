/*
 * Trap Handlers
 */

.extern trap_entry

.text

.macro TRAP_NOEC TRAPNUM
trap\TRAPNUM:
    # Push top of the trap frame
    pushq   %rax
    pushq    $\TRAPNUM
    pushq   %rax
    xorq    %rax, %rax
    movw    %ds, %ax
    pushq   %rax
    jmp trap_common
.endm

.macro TRAP_EC TRAPNUM
trap\TRAPNUM:
    # Push top of the trap frame
    pushq    $\TRAPNUM
    pushq   %rax
    xorq    %rax, %rax
    movw    %ds, %ax
    pushq   %rax
    jmp     trap_common
.endm

.globl trap_table
trap_table:
.quad trap0
.quad trap1
.quad trap2
.quad trap3
.quad trap4
.quad trap5
.quad trap6
.quad trap7
.quad trap8
.quad trap9
.quad trap10
.quad trap11
.quad trap12
.quad trap13
.quad trap14
.quad trap15
.quad trap16
.quad trap17
.quad trap18
.quad trap19
.quad trap20
.quad trap21
.quad trap22
.quad trap23
.quad trap24
.quad trap25
.quad trap26
.quad trap27
.quad trap28
.quad trap29
.quad trap30
.quad trap31
.quad trap32
.quad trap33
.quad trap34
.quad trap35
.quad trap36
.quad trap37
.quad trap38
.quad trap39
.quad trap40
.quad trap41
.quad trap42
.quad trap43
.quad trap44
.quad trap45
.quad trap46
.quad trap47
.quad trap48
.quad trap49
.quad trap50
.quad trap51
.quad trap52
.quad trap53
.quad trap54
.quad trap55
.quad trap56
.quad trap57
.quad trap58
.quad trap59
.quad trap60
.quad trap61
.quad trap62
.quad trap63

TRAP_NOEC 0     // DE
TRAP_NOEC 1     // DB
TRAP_NOEC 2     // NMI
TRAP_NOEC 3     // BP
TRAP_NOEC 4     // OF
TRAP_NOEC 5     // BR
TRAP_NOEC 6     // UD
TRAP_NOEC 7     // NM
TRAP_EC 8       // DF
TRAP_NOEC 9
TRAP_EC 10      // TS
TRAP_EC 11      // NP
TRAP_EC 12      // SS
TRAP_EC 13      // GP
TRAP_EC 14      // PF
TRAP_NOEC 15
TRAP_NOEC 16    // MF
TRAP_EC 17      // AC
TRAP_NOEC 18    // MC
TRAP_NOEC 19    // XF
TRAP_NOEC 20    // VE
TRAP_NOEC 21
TRAP_NOEC 22
TRAP_NOEC 23
TRAP_NOEC 24
TRAP_NOEC 25
TRAP_NOEC 26
TRAP_NOEC 27
TRAP_NOEC 28
TRAP_NOEC 29
TRAP_NOEC 30
TRAP_NOEC 31
TRAP_NOEC 32    // IRQ 0
TRAP_NOEC 33
TRAP_NOEC 34
TRAP_NOEC 35
TRAP_NOEC 36
TRAP_NOEC 37
TRAP_NOEC 38
TRAP_NOEC 39
TRAP_NOEC 40
TRAP_NOEC 41
TRAP_NOEC 42
TRAP_NOEC 43
TRAP_NOEC 44
TRAP_NOEC 45
TRAP_NOEC 46
TRAP_NOEC 47    // IRQ 15
TRAP_NOEC 48    // IRQ 16 (PCI)
TRAP_NOEC 49
TRAP_NOEC 50
TRAP_NOEC 51
TRAP_NOEC 52
TRAP_NOEC 53
TRAP_NOEC 54
TRAP_NOEC 55    // IRQ 23 (PCI)
TRAP_NOEC 56    // LAPIC Spurious
TRAP_NOEC 57    // LAPIC Error
TRAP_NOEC 58    // LAPIC Thermal
TRAP_NOEC 59
TRAP_NOEC 60    // System Call
TRAP_NOEC 61
TRAP_NOEC 62
TRAP_NOEC 63

trap_common:
    # Create the rest of the trap frame
    pushq   %rbx
    pushq   %rcx
    pushq   %rdx
    pushq   %rsi
    pushq   %rdi
    pushq   %rbp
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    # Pass the trap frame as an argument to trap_entry
    movq    %rsp, %rdi
    call    trap_entry
.globl trap_return
trap_return:
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rbp
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    popq    %rbx
    popq    %rax
    movw    %ax, %ds
    popq    %rax

    # Skip error code and vector number
    addq    $16, %rsp

    # Return to userspace
    iretq

.globl Trap_Pop
Trap_Pop:
    movq    %rdi, %rsp
    jmp trap_return

