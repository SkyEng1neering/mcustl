/**
 * @file mcustl_device_conf.h
 * @brief Test configuration mirroring the exciter-mini device deployment.
 *
 * Differs from mcustl_test_conf.h on the dimensions that turned out to
 * matter for hard-to-reproduce defrag bugs: 32-byte (cache-line)
 * alignment vs 8-byte, and the device's tight 256-entry tracker cap.
 *
 * Used by the mcustl_device_tests target so the same JSON-build-shape
 * tests run under a heap layout that closely matches the device.
 */

#ifndef MCUSTL_DEVICE_CONF_H
#define MCUSTL_DEVICE_CONF_H

/* Module set matches the device build. */
#define MCUSTL_USE_VECTOR       1
#define MCUSTL_USE_STRING       1
#define MCUSTL_USE_SMART_PTR    1
#define MCUSTL_USE_LIST         0
#define MCUSTL_USE_MAP          1
#define MCUSTL_USE_JSON         1

#define USE_SINGLE_HEAP_MEMORY

/* Match exciter-mini's mcustl_custom_conf.h. */
#define ALLOCATION_ALIGNMENT_BYTES  32U
#define MAX_NUM_OF_ALLOCATIONS      256UL

/* Device runs without freed-memory zeroing — leaves stale bytes in the
 * shifted-down trailing region. Mirror that so any "reads from freed
 * tail" UB is exposed here too. */
/* #define FILL_FREED_MEMORY_BY_NULLS true  // intentionally NOT defined */

#define mcustl_debug(...) ((void)0)

/* pthreads recursive mutex for thread safety (matches host test setup). */
#define USE_THREAD_SAFETY

#include <pthread.h>
#include <stdlib.h>

static inline pthread_mutex_t* mcustl_device_mutex_create(void) {
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

static inline void mcustl_device_mutex_delete(pthread_mutex_t* m) {
    if (m) { pthread_mutex_destroy(m); free(m); }
}

#define MCUSTL_MUTEX_TYPE             pthread_mutex_t*
#define MCUSTL_MUTEX_CREATE()         mcustl_device_mutex_create()
#define MCUSTL_MUTEX_DELETE(mutex)    mcustl_device_mutex_delete(mutex)
#define MCUSTL_MUTEX_LOCK(mutex)      do { if(mutex) pthread_mutex_lock(mutex); } while(0)
#define MCUSTL_MUTEX_UNLOCK(mutex)    do { if(mutex) pthread_mutex_unlock(mutex); } while(0)

#endif /* MCUSTL_DEVICE_CONF_H */
