/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>

#include <sys/kassert.h>
#include <sys/kmem.h>
#include <sys/ktime.h>
#include <sys/ktimer.h>
#include <sys/thread.h>
#include <sys/loader.h>
#include <sys/syscall.h>
#include <sys/disk.h>
#include <sys/vfs.h>
#include <sys/vfsuio.h>
#include <sys/nic.h>
#include <sys/sysctl.h>

Handle *Console_OpenHandle();

uint64_t
Syscall_Time()
{
    return KTime_GetEpochNS();
}

uint64_t
Syscall_GetPID()
{
    Thread *cur = Sched_Current();
    uint64_t pid = cur->proc->pid;

    Thread_Release(cur);

    return pid;
}

void
Syscall_Exit(uint64_t status)
{
    Thread *cur = Sched_Current();

    // Request each thread to exit

    // Wait for all threads to exit

    // Write exit code
    cur->proc->exitCode = status;

    // Exit this thread
    Sched_SetZombie(cur);
    Thread_Release(cur);
    Sched_Scheduler();

    // Should not return
    Panic("Returned to exited thread!\n");

    return;
}

uint64_t
Syscall_Spawn(uint64_t user_path, uint64_t user_argv)
{
    int status;
    char path[512];
    void *pg;
    char *arg;
    VNode *file;
    Process *proc;
    Thread *thr;
    Thread *cur;

    status = Copy_StrIn(user_path, &path, sizeof(path));
    if (status != 0)
	return SYSCALL_PACK(status, 0);

    Log(syscall, "Spawn(%s)\n", path);

    arg = PAlloc_AllocPage();
    if (!arg) {
	return SYSCALL_PACK(ENOMEM, 0);
    }

    /* Copy argument pointers */
    for (int i = 0; i < 8; i++) {
	uintptr_t off = sizeof(uintptr_t)*i;

	status = Copy_In(user_argv+off, arg+sizeof(uintptr_t)*(1+i), sizeof(uintptr_t));
	if (status != 0) {
	    PAlloc_Release(arg);
	    return SYSCALL_PACK(status, 0);
	}

	if (*(uintptr_t *)(arg+sizeof(uintptr_t)*(1+i)) == 0)
	    break;
    }

    /* Copy each argument in */
    char *argstart = arg+sizeof(uintptr_t)*8;
    for (int i = 1; i < 8; i++) {
	uintptr_t *str = (uintptr_t *)(arg+sizeof(uintptr_t)*i);
	if (*str == 0)
	    break;

	status = Copy_StrIn(*str, argstart, 256); // XXX: Make sure there's no overrun
	if (status != 0) {
	    PAlloc_Release(arg);
	    return SYSCALL_PACK(status, 0);
	}

	*str = (uintptr_t)argstart;
	argstart += strlen(argstart)+1;
    }

    pg = PAlloc_AllocPage();
    if (!pg) {
	PAlloc_Release(arg);
	return SYSCALL_PACK(ENOMEM, 0);
    }

    /* XXXFILLMEIN: Load the ELF headers into the page. */

    if (!Loader_CheckHeader(pg)) {
	VFS_Close(file);
	PAlloc_Release(pg);
	PAlloc_Release(arg);
	return SYSCALL_PACK(EINVAL, 0);
    }

    cur = Sched_Current();
    proc = Process_Create(cur->proc, path);
    thr = Thread_Create(proc);
    Thread_Release(cur);
    Log(syscall, "SPAWN %lx\n", thr);

    Handle *handle = Console_OpenHandle();
    Handle_Add(proc, handle);
    handle = Console_OpenHandle();
    Handle_Add(proc, handle);
    handle = Console_OpenHandle();
    Handle_Add(proc, handle);

    Loader_Load(thr, file, pg, 1024);

    /* Initialize the trap frame for entering into the process. */
    Thread_SetupUThread(thr, proc->entrypoint, MEM_USERSPACE_STKTOP - PGSIZE);

    /* Translate mapping for stack page */
    argstart = (char *)DMPA2VA(PMap_Translate(thr->space, MEM_USERSPACE_STKTOP - PGSIZE));
    uintptr_t offset = sizeof(uintptr_t)*8;

    /* XXXFILLMEIN: Export the argument array out to the new application. */

    Sched_SetRunnable(thr);

    return SYSCALL_PACK(0, proc->pid);
}

uint64_t
Syscall_Wait(uint64_t pid)
{
    uint64_t status;
    Thread *cur = Sched_Current();

    status = Process_Wait(cur->proc, pid);
    Thread_Release(cur);

    return status;
}

uint64_t
Syscall_MMap(uint64_t addr, uint64_t len, uint64_t prot)
{
    Thread *cur = Sched_Current();
    bool status;

    status = PMap_AllocMap(cur->space, addr, len, PTE_W);
    Thread_Release(cur);
    if (!status) {
	// XXX: Need to unmap PMap_Unmap(cur->space, addr, pgs);
	return 0;
    } else {
	return addr;
    }
}

uint64_t
Syscall_MUnmap(uint64_t addr, uint64_t len)
{
    Thread *cur = Sched_Current();
    uint64_t p;

    for (p = 0; p < len; p += PGSIZE)
    {
	// Free page
    }

    PMap_Unmap(cur->space, addr, len /= PGSIZE);
    Thread_Release(cur);

    return 0;
}

uint64_t
Syscall_MProtect(uint64_t addr, uint64_t len, uint64_t prot)
{
    //Thread *cur = Sched_Current();
    NOT_IMPLEMENTED();
    return 0;
}

uint64_t
Syscall_Read(uint64_t fd, uint64_t addr, uint64_t off, uint64_t length)
{
    uint64_t status;
    Thread *cur = Sched_Current();
    Handle *handle = Handle_Lookup(cur->proc, fd);

    if (handle == NULL) {
	status = -EBADF;
    } else {
	status = (handle->read)(handle, (void *)addr, off, length);
    }

    Thread_Release(cur);

    return status;
}

uint64_t
Syscall_Write(uint64_t fd, uint64_t addr, uint64_t off, uint64_t length)
{
    uint64_t status;
    Thread *cur = Sched_Current();
    Handle *handle = Handle_Lookup(cur->proc, fd);

    if (handle == NULL) {
	status = -EBADF;
    } else {
	status = (handle->write)(handle, (void *)addr, off, length);
    }

    Thread_Release(cur);

    return status;
}

uint64_t
Syscall_Flush(uint64_t fd)
{
    uint64_t status;
    Thread *cur = Sched_Current();
    Handle *handle = Handle_Lookup(cur->proc, fd);

    if (handle == NULL) {
	status = -EBADF;
    } else {
	status = (handle->flush)(handle);
    }

    Thread_Release(cur);

    return status;
}

// XXX: Cleanup
Handle *Console_OpenHandle();

uint64_t
Syscall_Open(uint64_t user_path, uint64_t flags)
{
    uint64_t handleNo;
    Thread *cur = Sched_Current();
    int status;
    char path[256];

    status = Copy_StrIn(user_path, &path, sizeof(path));
    if (status != 0) {
	Thread_Release(cur);
	return status;
    }

    if (strncmp("/dev/", path, 5) == 0) {
	if (strcmp("/dev/console", path) == 0) {
	    Handle *handle = Console_OpenHandle();
	    handleNo = Handle_Add(cur->proc, handle);
	    Thread_Release(cur);
	    return handleNo;
	}

	Thread_Release(cur);
	return -ENOENT;
    }

    Handle *handle;
    status = VFSUIO_Open(path, &handle);
    if (status != 0) {
	Thread_Release(cur);
	return status;
    }

    handleNo = Handle_Add(cur->proc, handle);
    Thread_Release(cur);
    return handleNo;
}

uint64_t
Syscall_Close(uint64_t fd)
{
    uint64_t status;
    Thread *cur = Sched_Current();
    Handle *handle = Handle_Lookup(cur->proc, fd);

    if (handle == NULL) {
	status = -EBADF;
    } else {
	status = (handle->close)(handle);
    }

    Thread_Release(cur);

    return status;
}

uint64_t
Syscall_Stat(uint64_t user_path, uint64_t user_stat)
{
    int status;
    char path[256];
    struct stat sb;

    status = Copy_StrIn(user_path, &path, sizeof(path));
    if (status != 0) {
	return status;
    }

    // VFS_Stat
    status = VFS_Stat(path, &sb);
    if (status != 0) {
	return status;
    }

    status = Copy_Out(&sb, user_stat, sizeof(struct stat));
    if (status != 0) {
	return status;
    }

    return 0;
}

uint64_t
Syscall_ReadDir(uint64_t fd, char *user_buf, size_t len, uintptr_t user_off)
{
    int status, rstatus;
    Thread *cur = Sched_Current();
    Handle *handle = Handle_Lookup(cur->proc, fd);
    uint64_t offset;

    if (handle == NULL) {
	Thread_Release(cur);
	return -EBADF;
    }

    status = Copy_In(user_off, &offset, sizeof(offset));
    if (status != 0) {
	Thread_Release(cur);
	return status;
    }

    if (handle->type != HANDLE_TYPE_FILE) {
	Thread_Release(cur);
	return -ENOTDIR;
    }

    rstatus = VFS_ReadDir(handle->vnode, user_buf, len, &offset);
    if (rstatus < 0) {
	Thread_Release(cur);
	return rstatus;
    }

    status = Copy_Out(&offset, user_off, sizeof(offset));
    if (status != 0) {
	Thread_Release(cur);
	return status;
    }

    Thread_Release(cur);

    return rstatus;
}

uint64_t
Syscall_ThreadCreate(uint64_t rip, uint64_t arg)
{
    uint64_t threadId;
    Thread *curThread = Sched_Current();
    Thread *newThread = Thread_UThreadCreate(curThread, rip, arg);

    Thread_Release(curThread);
    if (newThread == NULL) {
	return SYSCALL_PACK(ENOMEM, 0);
    }

    threadId = newThread->tid;
    Sched_SetRunnable(newThread);

    return SYSCALL_PACK(0, threadId);
}

uint64_t
Syscall_GetTID()
{
    Thread *cur = Sched_Current();
    uint64_t tid = cur->tid;

    Thread_Release(cur);

    return tid;
}

void
Syscall_ThreadExit(uint64_t status)
{
    Thread *cur = Sched_Current();

    // Encode this like POSIX
    cur->exitValue = status;

    Sched_SetZombie(cur);
    Semaphore_Release(&cur->proc->zombieSemaphore);
    Thread_Release(cur);
    Sched_Scheduler();

    // Should not return
    Panic("Returned to exited thread!\n");
}

static void
ThreadWakeupHelper(void *arg)
{
    Thread *thr = (Thread *)arg;

    Sched_SetRunnable(thr);
    KTimer_Release(thr->timerEvt);
    thr->timerEvt = NULL;
    Thread_Release(thr);
}

uint64_t
Syscall_ThreadSleep(uint64_t time)
{
    Thread *cur = Sched_Current();

    // If the sleep time is zero just yield
    if (time != 0) {
	Thread_Retain(cur);
	cur->timerEvt = KTimer_Create(time, ThreadWakeupHelper, cur);
	if (cur->timerEvt == NULL) {
	    Thread_Release(cur);
	    Thread_Release(cur);
	    return -ENOMEM;
	}

	Sched_SetWaiting(cur);
    }
    Sched_Scheduler();

    Thread_Release(cur);

    return 0;
}

uint64_t
Syscall_ThreadWait(uint64_t tid)
{
    uint64_t status;
    Thread *cur = Sched_Current();

    /*
     * Acquire the zombie semaphore see if the specified thread has exited or 
     * any thread if tid == 0.  If the specified thread hasn't exited wait 
     * again on the semaphore.  POSIX does not give any guarentees if multiple 
     * threads wait on the same thread and neither do we.
     *
     * As a precaution we call Sched_Scheduler to prevent looping on the 
     * semaphore acquire-release.
     */
    while (1) {
	Semaphore_Acquire(&cur->proc->zombieSemaphore);
	status = Thread_Wait(cur, tid);
	if (SYSCALL_ERRCODE(status) != EAGAIN) {
	    Thread_Release(cur);
	    return status;
	}
	Semaphore_Release(&cur->proc->zombieSemaphore);
	Sched_Scheduler();
    }
}

uint64_t
Syscall_NICStat(uint64_t nicNo, uint64_t user_stat)
{
    int status;
    NIC *nic;

    nic = NIC_GetByID(nicNo);
    if (nic == NULL) {
	return ENOENT;
    }

    status = Copy_Out(nic, user_stat, sizeof(NIC));
    if (status != 0) {
	return status;
    }

    return 0;
}

uint64_t
Syscall_NICSend(uint64_t nicNo, uint64_t user_mbuf)
{
    int status;
    NIC *nic;
    MBuf mbuf;

    status = Copy_In(user_mbuf, &mbuf, sizeof(mbuf));
    if (status != 0) {
	return SYSCALL_PACK(status, 0);
    }

    nic = NIC_GetByID(nicNo);
    if (nic == NULL) {
	return SYSCALL_PACK(ENOENT, 0);
    }

    // Pin Memory
    (nic->tx)(nic, &mbuf, NULL, NULL);
    // Unpin Memory

    return 0;
}

uint64_t
Syscall_NICRecv(uint64_t nicNo, uint64_t user_mbuf)
{
    int status;
    NIC *nic;
    MBuf mbuf;

    status = Copy_In(user_mbuf, &mbuf, sizeof(mbuf));
    if (status != 0) {
	return SYSCALL_PACK(status, 0);
    }

    nic = NIC_GetByID(nicNo);
    if (nic == NULL) {
	return SYSCALL_PACK(ENOENT, 0);
    }

    // Pin Memory
    (nic->rx)(nic, &mbuf, NULL, NULL);
    // Unpin Memory

    return 0;
}

uint64_t
Syscall_SysCtl(uint64_t user_node, uint64_t user_oldval, uint64_t user_newval)
{
    uint64_t status;
    char node[64];

    status = Copy_StrIn(user_node, &node, sizeof(node));
    if (status != 0) {
	return SYSCALL_PACK(status, 0);
    }

    uint64_t scType = SysCtl_GetType(node);
    if (scType == SYSCTL_TYPE_INVALID) {
	return SYSCALL_PACK(ENOENT, 0);
    }

    if (user_oldval != 0) {
	switch (scType) {
	    case SYSCTL_TYPE_STR: {
		SysCtlString *scStr = SysCtl_GetObject(node);
		status = Copy_Out(scStr, user_oldval, sizeof(*scStr));
		break;
	    }
	    case SYSCTL_TYPE_INT: {
		SysCtlInt *scInt = SysCtl_GetObject(node);
		status = Copy_Out(scInt, user_oldval, sizeof(*scInt));
		break;
	    }
	    case SYSCTL_TYPE_BOOL: {
		SysCtlBool *scBool = SysCtl_GetObject(node);
		status = Copy_Out(scBool, user_oldval, sizeof(scBool));
		break;
	    }
	    default: {
		status = EINVAL;
	    }
	}

	if (status != 0) {
	    return SYSCALL_PACK(status, 0);
	}
    }

    if (user_newval != 0) {
	switch (scType) {
	    case SYSCTL_TYPE_STR: {
		SysCtlString scStr;
		status = Copy_In(user_newval, &scStr, sizeof(scStr));
		if (status != 0) {
		    return SYSCALL_PACK(status, 0);
		}
		status = SysCtl_SetObject(node, (void *)&scStr);
		break;
	    }
	    case SYSCTL_TYPE_INT: {
		SysCtlInt scInt;
		status = Copy_In(user_newval, &scInt, sizeof(scInt));
		if (status != 0) {
		    return SYSCALL_PACK(status, 0);
		}
		status = SysCtl_SetObject(node, (void *)&scInt);
		break;
	    }
	    case SYSCTL_TYPE_BOOL: {
		SysCtlBool scBool;
		status = Copy_In(user_newval, &scBool, sizeof(scBool));
		if (status != 0) {
		    return SYSCALL_PACK(status, 0);
		}
		status = SysCtl_SetObject(node, (void *)&scBool);
		break;
	    }
	    default: {
		status = EINVAL;
	    }
	}
    }

    return SYSCALL_PACK(status, 0);
}

uint64_t
Syscall_FSMount(uint64_t user_mntpt, uint64_t user_device, uint64_t flags)
{
    return SYSCALL_PACK(ENOSYS, 0);
}

uint64_t
Syscall_FSUnmount(uint64_t user_mntpt)
{
    return SYSCALL_PACK(ENOSYS, 0);
}

uint64_t
Syscall_FSInfo(uint64_t user_fsinfo, uint64_t max)
{
    return SYSCALL_PACK(ENOSYS, 0);
}

uint64_t
Syscall_Entry(uint64_t syscall, uint64_t a1, uint64_t a2,
	      uint64_t a3, uint64_t a4, uint64_t a5)
{
    switch (syscall)
    {
	case SYSCALL_NULL:
	    return 0;
	case SYSCALL_TIME:
	    return Syscall_Time();
	case SYSCALL_GETPID:
	    return Syscall_GetPID();
	case SYSCALL_EXIT:
	    Syscall_Exit(a1);
	    return 0; // To eliminate warning
	case SYSCALL_SPAWN:
	    return Syscall_Spawn(a1, a2);
	case SYSCALL_WAIT:
	    return Syscall_Wait(a1);
	case SYSCALL_MMAP:
	    return Syscall_MMap(a1, a2, a3);
	case SYSCALL_MUNMAP:
	    return Syscall_MUnmap(a1, a2);
	case SYSCALL_MPROTECT:
	    return Syscall_MProtect(a1, a2, a3);
	case SYSCALL_READ:
	    return Syscall_Read(a1, a2, a3, a4);
	case SYSCALL_WRITE:
	    return Syscall_Write(a1, a2, a3, a4);
	case SYSCALL_FLUSH:
	    return Syscall_Flush(a1);
	case SYSCALL_OPEN:
	    return Syscall_Open(a1, a2);
	case SYSCALL_CLOSE:
	    return Syscall_Close(a1);
	case SYSCALL_STAT:
	    return Syscall_Stat(a1, a2);
	case SYSCALL_READDIR:
	    return Syscall_ReadDir(a1, (char *)a2, a3, a4);
	case SYSCALL_THREADCREATE:
	    return Syscall_ThreadCreate(a1, a2);
	case SYSCALL_GETTID:
	    return Syscall_GetTID();
	case SYSCALL_THREADEXIT:
	    Syscall_ThreadExit(a1);
	    return 0;
	case SYSCALL_THREADSLEEP:
	    return Syscall_ThreadSleep(a1);
	case SYSCALL_THREADWAIT:
	    return Syscall_ThreadWait(a1);
	case SYSCALL_NICSTAT:
	    return Syscall_NICStat(a1, a2);
	case SYSCALL_NICSEND:
	    return Syscall_NICSend(a1, a2);
	case SYSCALL_NICRECV:
	    return Syscall_NICRecv(a1, a2);
	case SYSCALL_SYSCTL:
	    return Syscall_SysCtl(a1, a2, a3);
	case SYSCALL_FSMOUNT:
	    return Syscall_FSMount(a1, a2, a3);
	case SYSCALL_FSUNMOUNT:
	    return Syscall_FSUnmount(a1);
	case SYSCALL_FSINFO:
	    return Syscall_FSInfo(a1, a2);
	default:
	    return SYSCALL_PACK(ENOSYS, 0);
    }
}

