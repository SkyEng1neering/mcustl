/**
 * @file mcustl_guard.h
 * @brief mcustl::heap_guard — RAII heap-mutex acquisition.
 *
 * Acquires the recursive mutex of an mcustl heap for the lifetime of the
 * guard. Use it to bracket *multi-step* reads (or any sequence of
 * operations) where the data must remain stable across the whole block —
 * specifically: you read a pointer / reference into a tracked container,
 * then use it past one or more allocator calls, possibly issued by other
 * threads.
 *
 * Background. mcustl's thread-safety guarantees that each individual
 * alloc/dfree call is atomic; the heap mutex is held only for the
 * duration of one such call. It does *not* protect a sequence like
 *
 *     const char* p = some_str.c_str();   // reads tracked data_, returns
 *     // … another thread dfree's something here, defrag relocates blocks …
 *     memcpy(dst, p, n);                  // p points at the OLD address
 *
 * because mcustl's tracker only updates the *registered* `data_` slot
 * after defrag — your local `p` is a stack value the tracker has never
 * heard of. heap_guard plugs that hole by holding the mutex across the
 * whole read+use sequence, blocking any other thread from running its
 * dfree → defrag in the gap.
 *
 * The mutex is recursive (FreeRTOS recursive semaphore / pthread
 * PTHREAD_MUTEX_RECURSIVE), so reentrant calls into mcustl operations
 * that take the same lock internally (push_back, insert, dfree, …) are
 * fine.
 *
 * Single-heap usage:
 * @code
 *   {
 *       mcustl::heap_guard g;             // locks the default heap
 *       const char* name = j.at("name").get_string().c_str();
 *       memcpy(buf, name, len);           // safe — no foreign defrag possible
 *   }                                      // unlocks on scope exit
 * @endcode
 *
 * Multi-heap usage:
 * @code
 *   {
 *       mcustl::heap_guard g(my_heap);    // locks my_heap specifically
 *       …
 *   }
 * @endcode
 *
 * Cost. With USE_THREAD_SAFETY off, lock/unlock are no-ops, the guard
 * compiles to nothing. With it on, the cost is the same as one mutex
 * take/give — tiny on FreeRTOS, free on bare metal.
 */

#ifndef MCUSTL_GUARD_H
#define MCUSTL_GUARD_H

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

namespace mcustl {

class heap_guard {
public:
#ifdef USE_SINGLE_HEAP_MEMORY
    /** @brief Lock the default global heap. */
    heap_guard() : heap_(nullptr), is_default_(true) {
        def_heap_lock();
    }
#endif

    /** @brief Lock a specific heap (multi-heap mode, or by-handle in
     *         single-heap mode if you already have the pointer). */
    explicit heap_guard(heap_t* heap) : heap_(heap), is_default_(false) {
        heap_lock(heap);
    }

    ~heap_guard() {
#ifdef USE_SINGLE_HEAP_MEMORY
        if (is_default_) { def_heap_unlock(); return; }
#endif
        heap_unlock(heap_);
    }

    heap_guard(const heap_guard&)            = delete;
    heap_guard& operator=(const heap_guard&) = delete;
    heap_guard(heap_guard&&)                 = delete;
    heap_guard& operator=(heap_guard&&)      = delete;

private:
    heap_t* heap_;
    bool    is_default_;
};

} /* namespace mcustl */

#endif /* MCUSTL_GUARD_H */
