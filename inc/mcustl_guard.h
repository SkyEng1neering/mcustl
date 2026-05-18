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

/**
 * @brief mcustl::tracked_this<T> — RAII pseudo-tracker for `*this`.
 *
 * Methods of heap-allocated containers (vector / map / list / string) face
 * the "stale this" problem: calling dfree may trigger defragmentation, which
 * relocates the heap block holding *this. The compiler's `this` register
 * snapshot still points at the OLD address, so any subsequent
 * `this->member` read or write lands in the wrong struct.
 *
 * tracked_this registers a stack-local pointer (`self`) as a *pseudo-tracker*
 * with the heap. Pseudo-trackers carry no allocation; defrag's value-shift
 * pass updates their stored pointer when the target heap address moves, so
 * `self` stays valid across any number of inner dfrees. The method body
 * uses `self->member` instead of `this->member` and the code remains
 * correct across defrags.
 *
 * Usage pattern:
 * @code
 *   bool vector<T>::reserve(uint32_t cap) {
 *       vector<T>* self = this;
 *       mcustl::tracked_this<vector<T>> _ts(self, this->alloc_mem_ptr);
 *       // ... use self->capacity_val, self->container_ptr, ... ...
 *   }
 * @endcode
 *
 * The convenience macro `MCUSTL_TRACKED_THIS(SelfT, heap_expr)` declares
 * both the `self` shadow and the RAII guard.
 */
template <typename T>
class tracked_this {
public:
    tracked_this(T*& self_slot, heap_t* heap) noexcept
        : slot_(&self_slot), heap_(heap) {
        if (heap_) {
            register_pseudo_tracker(heap_, reinterpret_cast<void**>(slot_));
        }
    }

    ~tracked_this() noexcept {
        if (heap_) {
            unregister_pseudo_tracker(heap_, reinterpret_cast<void**>(slot_));
        }
    }

    tracked_this(const tracked_this&)            = delete;
    tracked_this& operator=(const tracked_this&) = delete;
    tracked_this(tracked_this&&)                 = delete;
    tracked_this& operator=(tracked_this&&)      = delete;

private:
    T**     slot_;
    heap_t* heap_;
};

} /* namespace mcustl */

/* Convenience macro: declares both the `self` shadow pointer and the
 * tracked_this guard. Place at the top of every method that may trigger
 * defrag (anything that calls dfree directly or transitively). After this
 * macro, use `self->member` instead of `this->member`. */
#define MCUSTL_TRACKED_THIS(SelfT, heap_expr)                          \
    SelfT* self = this;                                                 \
    ::mcustl::tracked_this<SelfT> _mcustl_self_track_(self, (heap_expr))

namespace mcustl {

/**
 * @brief mcustl::tracked_heap_ptr — RAII pseudo-tracker for a stack-local
 * pointer that aliases a heap buffer.
 *
 * Solves the "stale stack snapshot" problem: code that takes a pointer into a
 * heap-tracked buffer (e.g. `const char* p = some_str.c_str()`) and then
 * triggers — directly or transitively — a dfree+defrag while still using `p`
 * will read garbage. mcustl's tracker updates the string's internal `data_`,
 * but the stack-local `p` snapshot is not a tracker and is not touched.
 *
 * tracked_heap_ptr registers `&p` as a *pseudo-tracker*. Pseudo-trackers
 * carry no allocation; defrag's value-shift pass updates their stored
 * pointer value whenever the heap bytes they alias move. After the guard's
 * scope ends, the slot is unregistered.
 *
 * Pass the heap that owns the buffer `*ptr_slot` points into. If null
 * (pre-allocation lazy state), the default heap is used — that's where
 * mcustl tracks the buffer in single-heap mode anyway. If `*ptr_slot` is
 * null, registration is skipped (nothing to track).
 *
 * Usage pattern:
 * @code
 *   bool MyWrapper::configure(const char* json, size_t len) {
 *       // Caller's json points into a mcustl::string. Any heap work
 *       // below may defrag and move that string's buffer — register
 *       // &json so mcustl keeps it valid.
 *       mcustl::tracked_heap_ptr _tp(mcustl_get_default_heap(), &json);
 *
 *       reallocBuffersForNewFftSize();   // triggers dfree → defrag
 *       return DelegateBase::configure(json, len);   // json still valid
 *   }
 * @endcode
 */
class tracked_heap_ptr {
public:
    tracked_heap_ptr(heap_t* h, const char** ptr_slot) noexcept
        : slot_(reinterpret_cast<void**>(const_cast<char**>(ptr_slot))) {
        if (!slot_ || !*slot_) return;
        heap_ = h ? h : mcustl_get_default_heap();
        if (!heap_) return;
        register_pseudo_tracker(heap_, slot_);
        armed_ = true;
    }
    ~tracked_heap_ptr() noexcept {
        if (armed_) {
            unregister_pseudo_tracker(heap_, slot_);
        }
    }
    tracked_heap_ptr(const tracked_heap_ptr&)            = delete;
    tracked_heap_ptr& operator=(const tracked_heap_ptr&) = delete;
    tracked_heap_ptr(tracked_heap_ptr&&)                 = delete;
    tracked_heap_ptr& operator=(tracked_heap_ptr&&)      = delete;

private:
    void**  slot_;
    heap_t* heap_  = nullptr;
    bool    armed_ = false;
};

} /* namespace mcustl */

#endif /* MCUSTL_GUARD_H */
