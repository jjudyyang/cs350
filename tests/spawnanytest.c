#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Castor Only
#include <sys/wait.h>
#include <syscall.h>

int
main(int argc, const char *argv[])
{
    const char *program;
    int pid[10];
    int status;

    printf("Spawn parallel test wait any: ");
    for (int i = 0; i < 10; i++) {
        program = (i % 2) ? "/bin/false" : "/bin/true";
	pid[i] = OSSpawn(program, &argv[0]);
	printf("spawn: %lx\n", pid[i]);
    }

    for (int i = 0; i < 10; i++) {
	wait(&status);
	if (!WIFEXITED(status)) {
	    printf("wait: process did not exit\n");
	    return 0;
	}

	if (WEXITSTATUS(status) != (i % 2)) {
	    printf("wait: unexpected return %lx\n", status);
	    return 0;
	}
    }

    printf("Success!\n");

    return 0;
}

