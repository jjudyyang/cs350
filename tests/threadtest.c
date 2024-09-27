
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Castor Only
#include <syscall.h>

void
threadEntry(uint64_t arg)
{
    printf("Hello From Thread %d\n", arg);
    OSThreadExit(arg);
}

int
main(int argc, const char *argv[])
{
    printf("Thread Test\n");
    OSThreadCreate((uintptr_t)&threadEntry, 2);
    OSThreadCreate((uintptr_t)&threadEntry, 3);
    OSThreadCreate((uintptr_t)&threadEntry, 4);
    
    int result = OSThreadWait(0);
    printf("Result %d\n", result);
    result = OSThreadWait(0);
    printf("Result %d\n", result);
    result = OSThreadWait(0);
    printf("Result %d\n", result);

    printf("Success!\n");

    return 0;
}

