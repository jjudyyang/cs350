
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#define test_assert(_expr) \
    if (!(_expr)) { \
        __assert(__func__, __FILE__, __LINE__, #_expr); \
    }


pthread_mutex_t mtx;
pthread_cond_t cnd;

void *
thread_simple(void *arg)
{
    printf("thread_simple %p!\n", arg);

    return arg;
}

void *
thread_lock(void *arg)
{
    int i;
    int status;

    for (i = 0; i < 100; i++) {
	status = pthread_mutex_lock(&mtx);
	test_assert(status == 0);
	pthread_yield();
	status = pthread_mutex_unlock(&mtx);
	test_assert(status == 0);
    }

    return NULL;
}

void *
thread_cond(void *arg)
{
    int i;
    int status;

    for (i = 0; i < 100; i++) {
	status = pthread_cond_wait(&cnd, NULL);
	test_assert(status == 0);
	status = pthread_cond_signal(&cnd);
	test_assert(status == 0);
    }

    return NULL;
}

int
main(int argc, const char *argv[])
{
    int i;
    int status;
    pthread_t thr;
    void *result;

    printf("PThread Test\n");

    // Simple thread Test
    printf("simple test: ");
    status = pthread_create(&thr, NULL, thread_simple, NULL);
    test_assert(status == 0);
    status = pthread_join(thr, &result);
    test_assert(status == 0);
    test_assert(result == NULL);
    printf("OK\n");

    // Return value Test
    printf("return value test: ");
    status = pthread_create(&thr, NULL, thread_simple, (void *)1);
    test_assert(status == 0);
    status = pthread_join(thr, &result);
    test_assert(status == 0);
    test_assert(result == (void *)1);
    printf("OK\n");

    // Mutex Test
    printf("simple mutex lock test: ");
    status = pthread_mutex_init(&mtx, NULL);
    test_assert(status == 0);
    status = pthread_mutex_lock(&mtx);
    test_assert(status == 0);
    status = pthread_mutex_unlock(&mtx);
    test_assert(status == 0);
    status = pthread_mutex_destroy(&mtx);
    test_assert(status == 0);
    printf("OK\n");

    // Mutex Contention Test
    printf("contended mutex lock test: ");
    pthread_mutex_init(&mtx, NULL);
    status = pthread_create(&thr, NULL, thread_lock, (void *)1);
    test_assert(status == 0);
    for (i = 0; i < 100; i++) {
	status = pthread_mutex_lock(&mtx);
	test_assert(status == 0);
	pthread_yield();
	pthread_mutex_unlock(&mtx);
	test_assert(status == 0);
    }
    status = pthread_join(thr, &result);
    test_assert(status == 0);
    status = pthread_mutex_destroy(&mtx);
    test_assert(status == 0);
    printf("OK\n");

    // Condition Variable Test
    printf("simple condition variable test: ");
    status = pthread_cond_init(&cnd, NULL);
    test_assert(status == 0);
    status = pthread_cond_signal(&cnd);
    test_assert(status == 0);
    status = pthread_cond_wait(&cnd, NULL);
    test_assert(status == 0);
    status = pthread_cond_destroy(&cnd);
    test_assert(status == 0);
    printf("OK\n");

    printf("threaded condition variable test: OK");
    status = pthread_cond_init(&cnd, NULL);
    test_assert(status == 0);
    for (i = 0; i < 100; i++) {
	status = pthread_cond_signal(&cnd);
	test_assert(status == 0);
	status = pthread_cond_wait(&cnd, NULL);
	test_assert(status == 0);
    }
    status = pthread_cond_destroy(&cnd);
    test_assert(status == 0);
    printf("\n");

    printf("Success!\n");

    return 0;
}

