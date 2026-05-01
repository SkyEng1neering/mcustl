/*
 * Copyright 2021 Alexey Vasilenko
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @file mcustl_pair.h
 * @brief mcustl::pair<A,B> - Lightweight pair template for embedded systems
 *
 * Stale-this protection
 * ---------------------
 * If A or B is a heap-aware mcustl type (anything that exposes
 * `get_mem_pointer()`), every constructor and assignment operator brackets
 * the underlying field-by-field copy/move with `mcustl::tracked_this<pair>`.
 * That keeps the pair's `*this` location valid across any inner dfree+defrag
 * triggered while assigning `first` (so the subsequent `second = ...` writes
 * to the relocated pair, not to the stale start address).
 *
 * For trivial pairs (e.g. `pair<int, int>`) the SFINAE detection returns no
 * heap and `tracked_this` becomes a zero-cost no-op.
 *
 * Note: heap-aware types must also be default-constructible — the value /
 * copy / move constructors default-init `first` and `second` in the init
 * list, then do the actual assignment in the body under `tracked_this`. This
 * preserves correctness across defrag (an init list cannot host the guard,
 * because it runs before the function body).
 */

#ifndef MCUSTL_PAIR_H
#define MCUSTL_PAIR_H

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

namespace mcustl {

namespace pair_detail {

/* SFINAE: prefer the overload that compiles only when t.get_mem_pointer() is
 * a valid expression returning heap_t*. Drops to the variadic fallback for
 * trivially-typed pair fields (int, char, ...) — those need no tracking. */
template<typename T>
inline auto try_get_heap(T& t, int) -> decltype(t.get_mem_pointer()) {
    return t.get_mem_pointer();
}

template<typename T>
inline heap_t* try_get_heap(T&, ...) {
    return nullptr;
}

/* Returns the first non-null heap discoverable from any of the supplied
 * pair-related references. Used to pick the heap to register the
 * pseudo-tracker against. */
template<typename A, typename B>
inline heap_t* heap_for(A& a, B& b) {
    if (heap_t* h = try_get_heap(a, 0)) return h;
    return try_get_heap(b, 0);
}

template<typename A, typename B>
inline heap_t* heap_for(A& a, B& b, const A& a2, const B& b2) {
    if (heap_t* h = try_get_heap(a, 0)) return h;
    if (heap_t* h = try_get_heap(b, 0)) return h;
    if (heap_t* h = try_get_heap(const_cast<A&>(a2), 0)) return h;
    return try_get_heap(const_cast<B&>(b2), 0);
}

} /* namespace pair_detail */

template<typename A, typename B>
struct pair {
    /* Anonymous unions wrap the two members so the compiler-emitted
     * implicit destructor is NOT generated (a struct with a union of
     * non-trivially-destructible types has no implicit dtor). That puts
     * lifetime under our control: we explicitly construct in every ctor
     * and destruct in ~pair under tracked_this, so a defrag fired by
     * ~B can relocate `self` and ~A still runs on the new pair address.
     *
     * Access to `first`/`second` is unchanged because anonymous unions
     * inject member names into the enclosing scope.
     *
     * Layout is identical to a plain `A first; B second;`: each anonymous
     * union with a single member has the same size+alignment as that
     * member. */
    union { A first; };
    union { B second; };

    /* Default ctor: explicitly construct each member; trivial init for
     * heap-aware types (default_heap pickup), no defrag concern. */
    pair() {
        new (static_cast<void*>(&first))  A();
        new (static_cast<void*>(&second)) B();
    }

    /* Value ctors: default-init fields in the init list, then assign in the
     * body under tracked_this so a defrag triggered by `first = a` doesn't
     * leave `&this->second` stale. */
    pair(const A& a, const B& b) {
        new (static_cast<void*>(&first))  A();
        new (static_cast<void*>(&second)) B();
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  a, b));
        self->first  = a;
        self->second = b;
    }

    pair(const A& a, B&& b) {
        new (static_cast<void*>(&first))  A();
        new (static_cast<void*>(&second)) B();
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  a, b));
        self->first  = a;
        self->second = static_cast<B&&>(b);
    }

    pair(A&& a, B&& b) {
        new (static_cast<void*>(&first))  A();
        new (static_cast<void*>(&second)) B();
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  a, b));
        self->first  = static_cast<A&&>(a);
        self->second = static_cast<B&&>(b);
    }

    /* Copy ctor: default-init then copy-assign each field under the guard. */
    pair(const pair& other) {
        new (static_cast<void*>(&first))  A();
        new (static_cast<void*>(&second)) B();
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  other.first, other.second));
        self->first  = other.first;
        self->second = other.second;
    }

    /* Move ctor: same pattern as copy. */
    pair(pair&& other) noexcept {
        new (static_cast<void*>(&first))  A();
        new (static_cast<void*>(&second)) B();
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  other.first, other.second));
        self->first  = static_cast<A&&>(other.first);
        self->second = static_cast<B&&>(other.second);
    }

    pair& operator=(const pair& other) {
        if (this != &other) {
            pair* self = this;
            ::mcustl::tracked_this<pair> _t(self,
                pair_detail::heap_for(self->first, self->second,
                                      other.first, other.second));
            self->first  = other.first;
            self->second = other.second;
        }
        return *this;
    }

    pair& operator=(pair&& other) noexcept {
        if (this != &other) {
            pair* self = this;
            ::mcustl::tracked_this<pair> _t(self,
                pair_detail::heap_for(self->first, self->second,
                                      other.first, other.second));
            self->first  = static_cast<A&&>(other.first);
            self->second = static_cast<B&&>(other.second);
        }
        return *this;
    }

    /* Explicit destructor with tracked_this — necessary because ~B() (run
     * first by reverse-declaration-order) can dfree+defrag, relocating
     * the pair away from the original `this`. Without tracking, the
     * compiler-emitted auto-destruction of `first` would then run on the
     * stale `this` address — operating on a freed (or reused) byte
     * region whose alloc_mem_ptr lookalike happens to be garbage from
     * the next allocation. Symptom: SEGV in heap_lock during
     * vector<char>::~vector inside ~string inside ~pair, with
     * `saved_heap` showing two adjacent uint32 values from another
     * vector that landed on top of the freed pair. Reproduced by
     * test_json.cpp::JsonParseAOO_MinimalRepro_2str3float.
     *
     * Pattern: drive both sub-destructions from the tracked `self`
     * pointer, then placement-new default-constructed instances back
     * into self's slots so the compiler's automatic member destruction
     * (which still runs on the original `this`) sees a default-init
     * byte pattern at whatever address it lands on — for default
     * mcustl-types, ~vector / ~string skip the heap_lock branch
     * entirely (container_ptr == NULL). */
    ~pair() {
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second));
        self->second.~B();
        self->first.~A();
    }

    bool operator==(const pair& other) const {
        return first == other.first && second == other.second;
    }
    bool operator!=(const pair& other) const { return !(*this == other); }
    bool operator<(const pair& other) const {
        if (first < other.first) return true;
        if (other.first < first) return false;
        return second < other.second;
    }
};

template<typename A, typename B>
pair<A,B> make_pair(const A& a, const B& b) {
    return pair<A,B>(a, b);
}

} // namespace mcustl

#endif // MCUSTL_PAIR_H
