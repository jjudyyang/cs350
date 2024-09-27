
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(int argc, const char *argv[])
{
    pid_t pid;
    int status;

    fputs("Init spawning shell\n", stdout);

    const char *args[] = { "/bin/shell", NULL };
    pid = spawn("/bin/shell", args);
    if (pid < 0) {
	printf("init: Could not spawn shell (%d)\n", errno);
    }

    while (1) {
	pid = wait(&status);
	if (pid != (pid_t)-1) {
	    printf("init: Zombie process %ld exited (%08x)\n", (long)pid, status);
	} else if (errno != ENOSYS) {
	    printf("init: wait() error (%d)\n", errno);
	}
    }
}

