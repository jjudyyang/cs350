
#
# Program entry point
#

.text

.globl _start
_start:
    call __pthread_init
    call main
    movq %rax, %rdi
    call OSExit
    int $3

