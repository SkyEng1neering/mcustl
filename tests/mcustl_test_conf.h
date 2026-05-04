/**
 * @file mcustl_test_conf.h
 * @brief mcustl configuration overrides for unit tests
 */

#ifndef MCUSTL_TEST_CONF_H
#define MCUSTL_TEST_CONF_H

/* Enable all modules */
#define MCUSTL_USE_VECTOR       1
#define MCUSTL_USE_STRING       1
#define MCUSTL_USE_SMART_PTR    1
#define MCUSTL_USE_LIST         1
#define MCUSTL_USE_MAP          1
#define MCUSTL_USE_JSON         1

/* Enable single heap for tests */
#define USE_SINGLE_HEAP_MEMORY

/* 8-byte alignment for 64-bit systems */
#define ALLOCATION_ALIGNMENT_BYTES  8U

/* Allow many allocations during JSON stress tests. */
#define MAX_NUM_OF_ALLOCATIONS  16384UL

/* Zero memory on free */
#define FILL_FREED_MEMORY_BY_NULLS  true

/* Disable debug output during tests */
#define mcustl_debug(...) ((void)0)

/* Enable thread safety for tests using pthreads recursive mutex */
#define USE_THREAD_SAFETY

#include <pthread.h>
#include <stdlib.h>

static inline pthread_mutex_t* mcustl_test_mutex_create(void) {
    pthread_mutex_t* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (m) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(m, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    return m;
}

static inline void mcustl_test_mutex_delete(pthread_mutex_t* m) {
    if (m) { pthread_mutex_destroy(m); free(m); }
}

#define MCUSTL_MUTEX_TYPE             pthread_mutex_t*
#define MCUSTL_MUTEX_CREATE()         mcustl_test_mutex_create()
#define MCUSTL_MUTEX_DELETE(mutex)    mcustl_test_mutex_delete(mutex)
#define MCUSTL_MUTEX_LOCK(mutex)      do { if(mutex) pthread_mutex_lock(mutex); } while(0)
#define MCUSTL_MUTEX_UNLOCK(mutex)    do { if(mutex) pthread_mutex_unlock(mutex); } while(0)

#endif /* MCUSTL_TEST_CONF_H */
