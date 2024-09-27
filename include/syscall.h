
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <sys/stat.h>
#include <sys/nic.h>
#include <sys/mbuf.h>
#include <sys/mount.h>

uint64_t OSTime();
uint64_t OSGetPID();
void OSExit(int status);
uint64_t OSSpawn(const char *path, const char *argv[]);
uint64_t OSWait(uint64_t pid);

// Memory
void *OSMemMap(void *addr, uint64_t len, int flags);
int OSMemUnmap(void *addr, uint64_t len);
int OSMemProtect(void *addr, uint64_t len, int flags);

// IO
int OSRead(uint64_t fd, void *addr, uint64_t off, uint64_t length);
int OSWrite(uint64_t fd, const void *addr, uint64_t off, uint64_t length);
int OSFlush(uint64_t fd);
uint64_t OSOpen(const char *path, uint64_t flags);
int OSClose(uint64_t fd);

// Directory
int OSStat(const char *path, struct stat *sb);
int OSReadDir(uint64_t fd, char *buf, size_t length, uint64_t *offset);

// Threads
int OSThreadCreate(uint64_t rip, uint64_t arg);
int OSGetTID();
int OSThreadExit(uint64_t status);
int OSThreadSleep(uint64_t time);
int OSThreadWait(uint64_t tid);

// Network
int OSNICStat(uint64_t nicNo, NIC *nic);
int OSNICSend(uint64_t nicNo, MBuf *mbuf);
int OSNICRecv(uint64_t nicNo, MBuf *mbuf);

// System
int OSSysCtl(const char *node, void *oldval, void *newval);
int OSFSMount(const char *mntpt, const char *device, uint64_t flags);
int OSFSUnmount(const char *mntpt);
int OSFSInfo(struct statfs *info, uint64_t max);

#endif /* __SYSCALL_H__ */

