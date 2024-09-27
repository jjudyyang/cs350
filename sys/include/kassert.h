
#ifndef __KASSERT_H__
#define __KASSERT_H__

#include <sys/cdefs.h>
#include <sys/sysctl.h>

#define ASSERT(_x) \
    if (!(_x)) { \
	Debug_Assert("ASSERT("#_x"): %s %s:%d\n", \
		__FUNCTION__, __FILE__, __LINE__); \
    }
#define NOT_IMPLEMENTED() \
    if (1) { \
	Debug_Assert("NOT_IMPLEMENTED(): %s %s:%d\n", \
		__FUNCTION__, __FILE__, __LINE__); \
    }
#define PANIC Panic

NO_RETURN void Panic(const char *str);

int kprintf(const char *fmt, ...);
NO_RETURN void Debug_Assert(const char *fmt, ...);

#define static_assert _Static_assert

// Alert
#define Alert(_module, _format, ...) kprintf(#_module ": " _format, ##__VA_ARGS__)
// Warning
#define Warning(_module, _format, ...) kprintf(#_module ": " _format, ##__VA_ARGS__)
// Normal Logging
#define Log(_module, _format, ...) \
    if (SYSCTL_GETINT(log_##_module) >= 1) { \
	kprintf(#_module ": " _format, ##__VA_ARGS__); \
    }
// Debug Logging
#define DLOG(_module, _format, ...) \
    if (SYSCTL_GETINT(log_##_module) >= 5) { \
	kprintf(#_module ": " _format, ##__VA_ARGS__); \
    }
// Verbose Logging
#define VLOG(_module, _format, ...) \
    if (SYSCTL_GETINT(log_##_module) >= 10) { \
	kprintf(#_module ": " _format, ##__VA_ARGS__); \
    }

#endif /* __KASSERT_H__ */

