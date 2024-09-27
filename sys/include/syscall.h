
#ifndef __SYS_SYSCALL_H__
#define __SYS_SYSCALL_H__

#define SYSCALL_NULL		0x00
#define SYSCALL_TIME		0x01
#define SYSCALL_GETPID		0x02
#define SYSCALL_EXIT		0x03
#define SYSCALL_SPAWN		0x04
#define SYSCALL_WAIT		0x05

// Memory
#define SYSCALL_MMAP		0x08
#define SYSCALL_MUNMAP		0x09
#define SYSCALL_MPROTECT	0x0A

// Stream
#define SYSCALL_READ		0x10
#define SYSCALL_WRITE		0x11
#define SYSCALL_FLUSH		0x12

// File
#define SYSCALL_OPEN		0x18
#define SYSCALL_CLOSE		0x19
#define SYSCALL_MOVE		0x1A
#define SYSCALL_DELETE		0x1B
#define SYSCALL_SETLENGTH	0x1C
#define SYSCALL_STAT		0x1D
#define SYSCALL_READDIR		0x1E

// IPC
#define SYSCALL_PIPE		0x20

// Threading
#define SYSCALL_THREADCREATE	0x30
#define SYSCALL_GETTID		0x31
#define SYSCALL_THREADEXIT	0x32
#define SYSCALL_THREADSLEEP	0x33
#define SYSCALL_THREADWAIT	0x34

// Network
#define SYSCALL_NICSTAT		0x40
#define SYSCALL_NICSEND		0x41
#define SYSCALL_NICRECV		0x42

// System
#define SYSCALL_SYSCTL		0x80
#define SYSCALL_FSMOUNT		0x81
#define SYSCALL_FSUNMOUNT	0x82
#define SYSCALL_FSINFO		0x83

uint64_t Syscall_Entry(uint64_t syscall, uint64_t a1, uint64_t a2,
		       uint64_t a3, uint64_t a4, uint64_t a5);

#define SYSCALL_PACK(_errcode, _val) (((uint64_t)_errcode << 32) | (_val))
#define SYSCALL_ERRCODE(_result) (_result >> 32)
#define SYSCALL_VALUE(_result) (_result & 0xFFFFFFFF)

#endif /* __SYS_SYSCALL_H__ */

