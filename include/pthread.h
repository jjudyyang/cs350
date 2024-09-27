
#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include <time.h>

#define PTHREAD_MUTEX_INITIALIZER	NULL
#define PTHREAD_COND_INITIALIZER	NULL

typedef struct pthread *pthread_t;
typedef struct pthread_attr *pthread_attr_t;
typedef struct pthread_barrier *pthread_barrier_t;
typedef struct pthread_barrierattr *pthread_barrierattr_t;
typedef struct pthread_mutex *pthread_mutex_t;
typedef struct pthread_mutexattr *pthread_mutexattr_t;
typedef struct pthread_cond *pthread_cond_t;
typedef struct pthread_condattr *pthread_condattr_t;

pthread_t pthread_self(void);
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		   void *(*start_routine)(void *), void *arg);
void pthread_exit(void *value_ptr);
int pthread_join(pthread_t thread, void **value_ptr);
void pthread_yield(void);

/*
 * Barriers
 */

int pthread_barrier_init(pthread_barrier_t *barrier,
			 const pthread_barrierattr_t *attr,
			 unsigned count);
int pthread_barrier_destroy(pthread_barrier_t *barrier);
int pthread_barrier_wait(pthread_barrier_t *barrier);

/*
 * Mutex
 */

int pthread_mutex_init(pthread_mutex_t *mutex,
		       const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/*
 * Reader/Writer Lock
 */

/*
 * Condition Variables
 */

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

#endif /* __PTHREAD_H__ */

