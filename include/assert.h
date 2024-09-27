
#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifndef NDEBUG

#define assert(_expr) \
    if (!(_expr)) { \
	__assert(__func__, __FILE__, __LINE__, #_expr); \
    }

#else

#define assert(_expr)

#endif

void __assert(const char *func, const char *file, int line, const char *expr);

#endif /* __ASSERT_H__ */

