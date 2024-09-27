
#include <stdio.h>
#include <time.h>

int
main(int argc, const char *argv[])
{
    time_t t = time(NULL);
    fputs(ctime(&t), stdout);
}

