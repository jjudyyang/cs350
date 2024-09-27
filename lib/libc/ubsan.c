// Minimal UBSan runtime (userspace)

#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#define STRLEN(literal) (sizeof(literal) - 1)

__no_ubsan
_Noreturn void __ubsan_handle(const char *func, size_t len) {
    OSWrite(STDERR_FILENO, func, 0, len);
    OSWrite(STDERR_FILENO, "()\n", 0, STRLEN("()\n"));
    OSExit(-1);

    UNREACHABLE();
}

#define UBSAN_HANDLER(name) \
    __no_ubsan \
    _Noreturn void __ubsan_handle_##name##_minimal(void) { \
	__ubsan_handle(__func__, STRLEN(__func__)); \
    }

UBSAN_HANDLER(add_overflow)
UBSAN_HANDLER(builtin_unreachable)
UBSAN_HANDLER(divrem_overflow)
UBSAN_HANDLER(function_type_mismatch)
UBSAN_HANDLER(load_invalid_value)
UBSAN_HANDLER(mul_overflow)
UBSAN_HANDLER(negate_overflow)
UBSAN_HANDLER(out_of_bounds)
UBSAN_HANDLER(pointer_overflow)
UBSAN_HANDLER(shift_out_of_bounds)
UBSAN_HANDLER(sub_overflow)
UBSAN_HANDLER(type_mismatch)
