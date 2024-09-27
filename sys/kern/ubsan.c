// Minimal UBSan runtime (kernel)

#include <stdint.h>
#include <sys/kassert.h>

#include "../dev/console.h"

__no_ubsan
_Noreturn void __ubsan_handle(const char *func) {
    Console_Puts(func);
    Panic(func);
}

#define UBSAN_HANDLER(name) \
    __no_ubsan \
    _Noreturn void __ubsan_handle_##name##_minimal(void) { \
	__ubsan_handle(__func__); \
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
