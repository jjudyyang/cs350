
#ifndef __CDEFS_H__
#define __CDEFS_H__

#include <stddef.h>

#define PACKED		__attribute__((__packed__))
#define PERCORE		__attribute__((section(".data.percore")))

#define INLINE		inline
#define ALWAYS_INLINE	__attribute__((__always_inline__))

#define NO_RETURN	__attribute__((noreturn))
#define UNREACHABLE	__builtin_unreachable

#define UNUSED		__attribute__((unused))

#define ROUNDUP(_x, _n) (((_x) + (_n) - 1) & ~((_n) - 1))

#define __MALLOC	__attribute__((__malloc__))

#if __has_extension(c_thread_safety_attributes)
#define __NO_LOCK_ANALYSIS	__attribute__((no_thread_safety_analysis))
#define __LOCKABLE		__attribute__((lockable))
#define __LOCK_EX(_x)		__attribute__((exclusive_lock_function(_x)))
#define __TRYLOCK_EX(_x)	__attribute__((exclusive_trylock_function(_x)))
#define __UNLOCK_EX(_x)		__attribute__((unlock_function(_x)))
#define __LOCK_EX_ASSERT(_x)	__attribute__((assert_exclusive_lock(_x)))
#define __GUARDED_BY(_x)	__attribute__((guarded_by(_x)))
#else
#define __NO_LOCK_ANALYSIS
#define __LOCKABLE
#define __LOCK_EX(_x)
#define __TRYLOCK_EX(_x)
#define __UNLOCK_EX(_x)
#define __LOCK_EX_ASSERT(_x)
#define __GUARDED_BY(_x)
#endif

#define __hidden	__attribute__((__visibility__("hidden")))

#define __printflike(_fmt, _var) __attribute__((__format__(__printf__, _fmt, _var)))

#define __no_ubsan	__attribute__((__no_sanitize__("undefined")))

#endif /* __CDEFS_H__ */

