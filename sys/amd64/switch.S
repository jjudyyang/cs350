/*
 * Trap Handlers
 */

#include <machine/asm.h>

.text

# switch(uint64_t *oldsp, uint64_t newsp)
# %rdi: oldsp
# %rsi: newsp
FUNC_BEGIN(switchstack)
    # Save callee saved registers of old thread
    pushq   %rbp
    pushq   %rdi
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    # Switch stack from old to new thread
    movq    %rsp, (%rdi)
    movq    %rsi, %rsp

    # Restore callee saved registers of new thread
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rdi
    popq    %rbp
    ret
FUNC_END(switchstack)

