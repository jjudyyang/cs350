
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <core/mutex.h>

#include <syscall.h>
#include <sys/syscall.h>

struct pthread_attr {
    uint64_t	_unused;
};

struct pthread {
    uint64_t	tid;
    int		error;			    // errno
    TAILQ_ENTRY(pthread)    threadTable;

    // Initialization
    void	*(*entry)(void *);
    void	*arg;

    // Termination
    void	*result;

    // Condition Variables
    TAILQ_ENTRY(pthread)    cvTable;
};

typedef TAILQ_HEAD(pthreadList, pthread) pthreadList;

#define THREAD_HASH_SLOTS	32

static CoreMutex __threadTableLock;
static pthreadList __threads[THREAD_HASH_SLOTS];

int *
__error(void)
{
    struct pthread *pth = pthread_self();

    return &(pth->error);
}

void
__pthread_init(void)
{
    int i;
    struct pthread *thr = (struct pthread *)malloc(sizeof(*thr));

    for (i = 0; i < THREAD_HASH_SLOTS; i++) {
	TAILQ_INIT(&__threads[i]);
    }

    if (thr == NULL) {
	abort();
    }

    thr->tid = OSGetTID();
    thr->error = 0;
    thr->entry = NULL;
    thr->arg = 0;

    CoreMutex_Init(&__threadTableLock);

    CoreMutex_Lock(&__threadTableLock);
    TAILQ_INSERT_HEAD(&__threads[thr->tid % THREAD_HASH_SLOTS], thr, threadTable);
    CoreMutex_Unlock(&__threadTableLock);
}

pthread_t
pthread_self(void)
{
    int tid = OSGetTID();
    struct pthread *thr;

    CoreMutex_Lock(&__threadTableLock);
    TAILQ_FOREACH(thr, &__threads[tid % THREAD_HASH_SLOTS], threadTable) {
	if (thr->tid == tid) {
	    CoreMutex_Unlock(&__threadTableLock);
	    return thr;
	}
    }
    CoreMutex_Unlock(&__threadTableLock);

    printf("pthread_self failed to find current thread!\n");
    abort();
}

void
pthreadCreateHelper(void *arg)
{
    struct pthread *thr = (struct pthread *)arg;

    thr->result = (thr->entry)(thr->arg);

    OSThreadExit(0);
}

int
pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	       void *(*start_routine)(void *), void *arg)
{
    uint64_t status;
    struct pthread *thr;

    thr = malloc(sizeof(*thr));
    if (!thr) {
	return EAGAIN;
    }

    memset(thr, 0, sizeof(*thr));

    thr->entry = start_routine;
    thr->arg = arg;

    status = OSThreadCreate((uintptr_t)&pthreadCreateHelper, (uint64_t)thr);
    if (SYSCALL_ERRCODE(status) != 0) {
	free(thr);
	return SYSCALL_ERRCODE(status);
    }

    thr->tid = SYSCALL_VALUE(status);

    CoreMutex_Lock(&__threadTableLock);
    TAILQ_INSERT_HEAD(&__threads[thr->tid % THREAD_HASH_SLOTS], thr, threadTable);
    CoreMutex_Unlock(&__threadTableLock);

    *thread = thr;

    return 0;
}

void
pthread_exit(void *value_ptr)
{
    struct pthread *thr = pthread_self();

    thr->result = value_ptr;

    OSThreadExit(0);
}

int
pthread_join(pthread_t thread, void **value_ptr)
{
    struct pthread *thr = thread;
    uint64_t status = OSThreadWait(0);//thr->tid);
    if (SYSCALL_ERRCODE(status) != 0) {
	return status;
    }

    *value_ptr = (void *)thr->result;

    CoreMutex_Lock(&__threadTableLock);
    TAILQ_REMOVE(&__threads[thr->tid % THREAD_HASH_SLOTS], thr, threadTable);
    CoreMutex_Unlock(&__threadTableLock);

    // Cleanup
    free(thr);

    return 0;
}

void
pthread_yield(void)
{
    OSThreadSleep(0);
}

/*
 * Barriers
 */

int
pthread_barrier_init(pthread_barrier_t *barrier,
		     const pthread_barrierattr_t *attr,
		     unsigned count)
{
    return EINVAL;
}

int
pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    return EINVAL;
}

int
pthread_barrier_wait(pthread_barrier_t *barrier)
{
    return EINVAL;
}

/*
 * Mutex
 */

struct pthread_mutexattr {
    uint64_t	_unused;
};

struct pthread_mutex {
    uint64_t	lock;
};

int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    struct pthread_mutex *mtx = (struct pthread_mutex *)malloc(sizeof(*mtx));

    if (mtx == NULL) {
	return ENOMEM;
    }

    mtx->lock = 0;
    *mutex = mtx;

    return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    struct pthread_mutex *mtx = *mutex;

    if (mtx == NULL) {
	return EINVAL;
    } else if (mtx->lock == 1) {
	return EBUSY;
    } else {
	*mutex = NULL;
	free(mtx);
	return 0;
    }
}

int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
    struct pthread_mutex *mtx;
    
    if (*mutex == NULL) {
	int status = pthread_mutex_init(mutex, NULL);
	if (status != 0)
	    return status;
    }

    mtx = *mutex;

    // XXX: Should have the kernel wait kernel instead of yielding
    while (__sync_lock_test_and_set(&mtx->lock, 1) == 1) {
	OSThreadSleep(0);
    }

    return 0;
}

int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    struct pthread_mutex *mtx;

    if (*mutex == NULL) {
	int status = pthread_mutex_init(mutex, NULL);
	if (status != 0)
	    return status;
    }

    mtx = *mutex;

    if (__sync_lock_test_and_set(&mtx->lock, 1) == 1) {
	return EBUSY;
    } else {
	// Lock acquired
	return 0;
    }
}

int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    struct pthread_mutex *mtx;

    if (*mutex == NULL) {
	int status = pthread_mutex_init(mutex, NULL);
	if (status != 0)
	    return status;
    }

    mtx = *mutex;

    __sync_lock_release(&mtx->lock);
    // XXX: Wakeup a sleeping thread

    return 0;
}


/*
 * Reader/Writer Lock
 */

/*
 * Condition Variables
 */

struct pthread_condattr {
    uint64_t	_unused;
};

struct pthread_cond {
    CoreMutex	mtx;
    uint64_t	enter;
    uint64_t	exit;
};

int
pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    struct pthread_cond *cnd = (struct pthread_cond *)malloc(sizeof(*cnd));

    if (cnd == NULL) {
	return ENOMEM;
    }

    CoreMutex_Init(&cnd->mtx);
    cnd->enter = 0;
    cnd->exit = 0;

    *cond = cnd;

    return 0;
}

int
pthread_cond_destroy(pthread_cond_t *cond)
{
    struct pthread_cond *cnd = *cond;

    *cond = NULL;
    free(cnd);

    return 0;
}

int
pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    int status;
    struct pthread_cond *cnd;
    uint64_t level;

    if (*cond == NULL) {
	status = pthread_cond_init(cond, NULL);
	if (status != 0)
	    return status;
    }
    cnd = *cond;

    if (mutex) {
	status = pthread_mutex_unlock(mutex);
	if (status != 0)
	    return status;
    }

    CoreMutex_Lock(&cnd->mtx);
    level = cnd->enter;
    cnd->enter++;
    CoreMutex_Unlock(&cnd->mtx);

    while (level >= cnd->exit) {
	OSThreadSleep(0);
    }

    if (mutex) {
	status = pthread_mutex_lock(mutex);
	if (status != 0)
	    return status;
    }

    return 0;
}

int
pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{
    int status = 0;
    int rstatus = 0;
    struct pthread_cond *cnd;
    uint64_t level;
    uint64_t endtime = abstime->tv_sec * 1000000000 + abstime->tv_nsec;

    if (*cond == NULL) {
	status = pthread_cond_init(cond, NULL);
	if (status != 0)
	    return status;
    }
    cnd = *cond;

    if (mutex) {
	status = pthread_mutex_unlock(mutex);
	if (status != 0)
	    return status;
    }

    CoreMutex_Lock(&cnd->mtx);
    level = cnd->enter;
    cnd->enter++;
    CoreMutex_Unlock(&cnd->mtx);

    while (level >= cnd->exit) {
	OSThreadSleep(0);

	if (endtime < OSTime()) {
	    rstatus = ETIMEDOUT;
	    break;
	}
    }

    if (mutex) {
	status = pthread_mutex_lock(mutex);
	if (status != 0)
	    return status;
    }

    return rstatus;
}


int
pthread_cond_signal(pthread_cond_t *cond)
{
    struct pthread_cond *cnd;

    if (*cond == NULL) {
	int status = pthread_cond_init(cond, NULL);
	if (status != 0)
	    return status;
    }
    cnd = *cond;

    CoreMutex_Lock(&cnd->mtx);
    cnd->exit++;
    CoreMutex_Unlock(&cnd->mtx);

    return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *cond)
{
    struct pthread_cond *cnd;

    if (*cond == NULL) {
	int status = pthread_cond_init(cond, NULL);
	if (status != 0)
	    return status;
    }
    cnd = *cond;

    CoreMutex_Lock(&cnd->mtx);
    cnd->exit = cnd->enter;
    CoreMutex_Unlock(&cnd->mtx);

    return 0;
}

