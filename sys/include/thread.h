
#ifndef __SYS_THREAD_H__
#define __SYS_THREAD_H__

#include <sys/queue.h>
#include <sys/handle.h>
#include <sys/ktimer.h>
#include <sys/waitchannel.h>

struct Thread;
typedef struct Thread Thread;
struct Process;
typedef struct Process Process;

#include <sys/semaphore.h>
#include <sys/mutex.h>
#include <sys/cv.h>

#include <machine/pmap.h>
#include <machine/thread.h>

typedef TAILQ_HEAD(ProcessQueue, Process) ProcessQueue;
typedef TAILQ_HEAD(ThreadQueue, Thread) ThreadQueue;

#define SCHED_STATE_NULL	0
#define SCHED_STATE_RUNNABLE	1
#define SCHED_STATE_RUNNING	2
#define SCHED_STATE_WAITING	3
#define SCHED_STATE_ZOMBIE	4

typedef struct Thread {
    ThreadArch		arch;
    AS			*space;
    uintptr_t		kstack;
    uintptr_t		ustack;
    uint64_t		tid;
    uint64_t		refCount;
    // Process
    struct Process	*proc;
    TAILQ_ENTRY(Thread)	threadList;
    // Scheduler
    int			schedState;
    TAILQ_ENTRY(Thread)	schedQueue;
    KTimerEvent		*timerEvt;	// Timer event for wakeups
    uintptr_t		exitValue;
    TAILQ_ENTRY(Thread)	semaQueue;	// Semaphore Queue
    // Wait Channels
    WaitChannel		*chan;
    TAILQ_ENTRY(Thread)	chanQueue;
    // Statistics
    uint64_t		ctxSwitches;
    uint64_t		userTime;
    uint64_t		kernTime;
    uint64_t		waitTime;
    uint64_t		waitStart;
} Thread;

#define PROCESS_HANDLE_SLOTS	128
#define PROCESS_TITLE_LENGTH	128

#define PROC_STATE_NULL		0
#define PROC_STATE_READY	1
#define PROC_STATE_ZOMBIE	2

typedef struct Process {
    uint64_t			pid;
    AS				*space;
    Spinlock			lock;
    uintptr_t			entrypoint;
    uint64_t			nextThreadID;
    uintptr_t			ustackNext;	// Next user stack
    TAILQ_ENTRY(Process)	processList;
    uint64_t			refCount;
    char			title[PROCESS_TITLE_LENGTH];
    int				procState;
    uint64_t			exitCode;
    // Process
    Process			*parent;
    TAILQ_ENTRY(Process)	siblingList;
    ProcessQueue		childrenList;
    ProcessQueue		zombieProc;
    Mutex			zombieProcLock;
    CV				zombieProcCV;
    CV				zombieProcPCV;
    // Threads
    uint64_t			threads;
    ThreadQueue			threadList;
    Semaphore			zombieSemaphore;
    ThreadQueue			zombieQueue;
    // Handles
    uint64_t			nextFD;
    HandleQueue			handles[PROCESS_HANDLE_SLOTS];
} Process;

// General
void Thread_Init();
void Thread_InitAP();

// Process functions
Process *Process_Create(Process *parent, const char *title);
Process *Process_Lookup(uint64_t pid);
void Process_Retain(Process *proc);
void Process_Release(Process *proc);
uint64_t Process_Wait(Process *proc, uint64_t pid);

#define TID_ANY		0xFFFFFFFF

// Thread functions
Thread *Thread_Create(Process *proc);
Thread *Thread_KThreadCreate(void (*f)(void*), void *arg);
Thread *Thread_UThreadCreate(Thread *oldThr, uint64_t rip, uint64_t arg);
Thread *Thread_Lookup(Process *proc, uint64_t tid);
void Thread_Retain(Thread *thr);
void Thread_Release(Thread *thr);
uint64_t Thread_Wait(Thread *thr, uint64_t tid);

// Scheduler functions
Thread *Sched_Current();
void Sched_SetRunnable(Thread *thr);
void Sched_SetWaiting(Thread *thr);
void Sched_SetZombie(Thread *thr);
void Sched_Scheduler();

// Debugging
void Process_Dump(Process *proc);
void Thread_Dump(Thread *thr);

// Platform functions
void Thread_InitArch(Thread *thr);
void Thread_SetupKThread(Thread *thr, void (*f)(),
			 uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);
void Thread_SetupUThread(Thread *thr, uint64_t rip, uint64_t arg);
void Thread_SwitchArch(Thread *oldthr, Thread *newthr);

// Handle Functions
void Handle_Init(Process *proc);
void Handle_Destroy(Process *proc);
uint64_t Handle_Add(Process *proc, Handle *handle);
void Handle_Remove(Process *proc, Handle *handle);
Handle *Handle_Lookup(Process *proc, uint64_t fd);

// Copy_In/Copy_Out Functions
int Copy_In(uintptr_t fromuser, void *tokernel, uintptr_t len);
int Copy_Out(void *fromkernel, uintptr_t touser, uintptr_t len);
int Copy_StrIn(uintptr_t fromuser, void *tokernel, uintptr_t len);
int Copy_StrOut(void *fromkernel, uintptr_t touser, uintptr_t len);

#endif /* __SYS_THREAD_H__ */

