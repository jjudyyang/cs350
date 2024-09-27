/*
 * System Call
 */

#include <machine/asm.h>

.text

FUNC_BEGIN(syscall)
    int $60
    ret
FUNC_END(syscall)

