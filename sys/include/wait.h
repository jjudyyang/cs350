
#ifndef __SYS_WAIT_H__
#define __SYS_WAIT_H__

#include <sys/cdefs.h>

/* Extract the status code from the status */
#define WEXITSTATUS(_x)	(_x & 0x000000ff)
#define _WSTATUS(_x) ((_x & 0x0000ff00) >> 8)
/* Exited gracefully */
#define WIFEXITED(_x) (_WSTATUS(_x) == 0)
/* Extract the pid from the status (Castor specific) */
#define WGETPID(_x)	((pid_t)(_x >> 16))

/* Flags to waitpid  */
#define WAIT_ANY	0

#ifndef _KERNEL
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
#endif

#endif /* __SYS_WAIT_H__ */

