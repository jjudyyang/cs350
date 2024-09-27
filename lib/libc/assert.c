
#include <stdio.h>
#include <stdlib.h>

void
__assert(const char *func, const char *file, int line, const char *expr)
{
    fprintf(stderr, "Assert (%s): %s %s:%d\n", expr, func, file, line);
    abort();
}

