
#ifndef __MACHINE_THREAD_H__
#define __MACHINE_THREAD_H__

#include <machine/amd64.h>
#include <machine/amd64op.h>

typedef struct ThreadArchStackFrame {
    uint64_t		r15;
    uint64_t		r14;
    uint64_t		r13;
    uint64_t		r12;
    uint64_t		rbx;
    uint64_t		rdi; // First argument
    uint64_t		rbp;
    uint64_t		rip;
} ThreadArchStackFrame;

typedef struct ThreadArch {
    XSAVEArea		xsa;
    bool		useFP;
    uint64_t		rsp;
} ThreadArch;

#endif /* __MACHINE_THREAD_H__ */

