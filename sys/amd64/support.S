/*
 * Support Functions
 */

#include <errno.h>
#include <machine/asm.h>

.text

// copy_unsafe(to, from, len)
FUNC_BEGIN(copy_unsafe)
    movq %rdx, %rcx
    rep movsb
.globl copy_unsafe_done
copy_unsafe_done:
    xorq %rax, %rax
    retq
.globl copy_unsafe_fault
copy_unsafe_fault:
    movq $EFAULT, %rax
    retq
FUNC_END(copy_unsafe)

// copystr_unsafe(to, from, len)
FUNC_BEGIN(copystr_unsafe)
    movq %rdx, %rcx
1:
    decq %rcx
    jz copystr_unsafe_toolong
    lodsb
    stosb
    orb %al, %al
    jnz 1b
.globl copystr_unsafe_done
copystr_unsafe_done:
    xorq %rax, %rax
    retq
.globl copystr_unsafe_fault
copystr_unsafe_fault:
    movq $EFAULT, %rax
    retq
.globl copystr_unsafe_toolong
    copystr_unsafe_toolong:
    movq $ENAMETOOLONG, %rax
    retq
FUNC_END(copystr_unsafe)

