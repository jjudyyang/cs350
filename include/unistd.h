
#ifndef __UNISTD_H__
#define __UNISTD_H__

#define STDIN_FILENO		0
#define STDOUT_FILENO		1
#define STDERR_FILENO		2

struct timeval;

int syscall(int number, ...);
unsigned int sleep(unsigned int seconds);
pid_t spawn(const char *path, const char *argv[]);

#endif /* __UNISTD_H__ */

