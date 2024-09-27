
#include <stdbool.h>
#include <stdint.h>

#include <sys/syscall.h>
#include <syscall.h>

uint64_t syscall(int num, ...);

uint64_t
OSTime()
{
    return syscall(SYSCALL_TIME);
}

void
OSExit(int status)
{
    syscall(SYSCALL_EXIT, status);
}

uint64_t
OSGetPID()
{
    return syscall(SYSCALL_GETPID);
}

uint64_t
OSSpawn(const char *path, const char *argv[])
{
    return syscall(SYSCALL_SPAWN, path, argv);
}

uint64_t
OSWait(uint64_t pid)
{
    return syscall(SYSCALL_WAIT, pid);
}

void *
OSMemMap(void *addr, uint64_t len, int flags)
{
    return (void *)syscall(SYSCALL_MMAP, addr, len, flags);
}

int
OSMemUnmap(void *addr, uint64_t len)
{
    return syscall(SYSCALL_MUNMAP, addr, len);
}

int
OSMemProtect(void *addr, uint64_t len, int flags)
{
    return syscall(SYSCALL_MPROTECT, addr, len, flags);
}

int
OSRead(uint64_t fd, void *addr, uint64_t off, uint64_t length)
{
    return syscall(SYSCALL_READ, fd, addr, off, length);
}

int
OSWrite(uint64_t fd, const void *addr, uint64_t off, uint64_t length)
{
    return syscall(SYSCALL_WRITE, fd, addr, off, length);
}

int
OSFlush(uint64_t fd)
{
    return syscall(SYSCALL_FLUSH, fd);
}

uint64_t
OSOpen(const char *path, uint64_t flags)
{
    return syscall(SYSCALL_OPEN, path, flags);
}

int
OSClose(uint64_t fd)
{
    return syscall(SYSCALL_CLOSE, fd);
}

int
OSStat(const char *path, struct stat *sb)
{
    return syscall(SYSCALL_STAT, path, sb);
}

int
OSReadDir(uint64_t fd, char *buf, size_t length, uint64_t *offset)
{
    return syscall(SYSCALL_READDIR, fd, buf, length, offset);
}

int
OSPipe(uint64_t fd[2])
{
    return syscall(SYSCALL_PIPE, &fd[0]);
}

int
OSThreadCreate(uint64_t rip, uint64_t arg)
{
    return syscall(SYSCALL_THREADCREATE, rip, arg);
}

int
OSGetTID()
{
    return syscall(SYSCALL_GETTID);
}

int
OSThreadExit(uint64_t status)
{
    return syscall(SYSCALL_THREADEXIT, status);
}

int
OSThreadSleep(uint64_t time)
{
    return syscall(SYSCALL_THREADSLEEP, time);
}

int
OSThreadWait(uint64_t tid)
{
    return syscall(SYSCALL_THREADWAIT, tid);
}

int
OSNICStat(uint64_t nicNo, NIC *nic)
{
    return syscall(SYSCALL_NICSTAT, nicNo, nic);
}

int
OSNICSend(uint64_t nicNo, MBuf *mbuf)
{
    return syscall(SYSCALL_NICSEND, nicNo, mbuf);
}

int
OSNICRecv(uint64_t nicNo, MBuf *mbuf)
{
    return syscall(SYSCALL_NICRECV, nicNo, mbuf);
}

int
OSSysCtl(const char *node, void *oldvar, void *newvar)
{
    return syscall(SYSCALL_SYSCTL, node, oldvar, newvar);
}

int
OSFSMount(const char *mntpt, const char *device, uint64_t flags)
{
    return syscall(SYSCALL_FSMOUNT, mntpt, device, flags);
}

int
OSFSUnmount(const char *mntpt)
{
    return syscall(SYSCALL_FSUNMOUNT, mntpt);
}

int
OSFSInfo(struct statfs *info, uint64_t max)
{
    return syscall(SYSCALL_FSINFO, info, max);
}

