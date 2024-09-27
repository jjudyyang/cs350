
#include <stdint.h>
#include <stdlib.h>

#include <sys/cdefs.h>
#include <syscall.h>

NO_RETURN void
abort()
{
    OSExit(-1);

    UNREACHABLE();
}

