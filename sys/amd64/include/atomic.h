
#ifndef __ATOMIC_H__
#define __ATOMIC_H__

static INLINE uint64_t
atomic_swap_uint32(volatile uint32_t *dst, uint32_t newval)
{
    asm volatile("lock; xchgl %0, %1;"
	    : "+m" (*dst), "+r" (newval));

    return newval;
}

static INLINE uint64_t
atomic_swap_uint64(volatile uint64_t *dst, uint64_t newval)
{
    asm volatile("lock; xchgq %0, %1;"
	    : "+m" (*dst), "+r" (newval));

    return newval;
}

static inline void
atomic_set_uint64(volatile uint64_t *dst, uint64_t newval)
{
    *dst = newval;
}

#endif /* __ATOMIC_H__ */

