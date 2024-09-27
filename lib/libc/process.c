
#include <stdint.h>

#include <sys/cdefs.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <syscall.h>

unsigned int
sleep(unsigned int seconds)
{
    OSThreadSleep(seconds);

    // Should return left over time if woke up early
    return 0;
}

pid_t
spawn(const char *path, const char *argv[])
{
    uint64_t status = OSSpawn(path, argv);

    if (SYSCALL_ERRCODE(status) != 0) {
	errno = SYSCALL_ERRCODE(status);
	return -1;
    }

    return (pid_t)SYSCALL_VALUE(status);
}

pid_t
waitpid(pid_t pid, int *status, UNUSED int options)
{
    uint64_t wstatus = OSWait(pid);

    if (SYSCALL_ERRCODE(wstatus) != 0) {
	errno = SYSCALL_ERRCODE(wstatus);
	return -1;
    }

    *status = SYSCALL_VALUE(wstatus) & 0x0000ffff;

    return WGETPID(SYSCALL_VALUE(wstatus));
}

pid_t
wait(int *status)
{
    return waitpid(WAIT_ANY, status, 0);
}

