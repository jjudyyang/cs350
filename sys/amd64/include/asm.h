/*
 * Assembly Macros
 */

#define FUNC_BEGIN(fname)   .p2align 4, 0x90; .global fname; \
                            .type fname, @function; \
                            fname:

#define FUNC_END(fname)     .size fname, . - fname

