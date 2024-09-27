
#include <stdint.h>
#include <stdlib.h>

#include <syscall.h>

struct atexit_cb {
    struct atexit_cb *next;
    void (*cb)(void);
};

static uint64_t _atexit_count = 0;
static struct atexit_cb _atexits[32]; // POSIX requires at least 32 atexit functions
static struct atexit_cb *_atexit_last = NULL;

int
atexit(void (*function)(void))
{
    if (_atexit_count < 32) {
	struct atexit_cb *prev = _atexit_last;

	_atexits[_atexit_count].cb = function;
	_atexits[_atexit_count].next = prev;

	_atexit_last = &_atexits[_atexit_count];
	_atexit_count++;
    } else {
	// XXX: Support malloc
	return -1;
    }

    return 0;
}

void
exit(int status)
{
    while (_atexit_last != NULL) {
	(_atexit_last->cb)();
	_atexit_last = _atexit_last->next;
    }

    OSExit(status & 0x00ff);

    __builtin_unreachable();
}

