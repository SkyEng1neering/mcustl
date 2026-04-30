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
    A first;
    B second;

    /* Default ctor: trivial init, no defrag concern. */
    pair() : first(), second() {}

    /* Value ctors: default-init fields in the init list, then assign in the
     * body under tracked_this so a defrag triggered by `first = a` doesn't
     * leave `&this->second` stale. */
    pair(const A& a, const B& b) : first(), second() {
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  a, b));
        self->first  = a;
        self->second = b;
    }

    pair(const A& a, B&& b) : first(), second() {
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  a, b));
        self->first  = a;
        self->second = static_cast<B&&>(b);
    }

    pair(A&& a, B&& b) : first(), second() {
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  a, b));
        self->first  = static_cast<A&&>(a);
        self->second = static_cast<B&&>(b);
    }

    /* Copy ctor: default-init then copy-assign each field under the guard. */
    pair(const pair& other) : first(), second() {
        pair* self = this;
        ::mcustl::tracked_this<pair> _t(self,
            pair_detail::heap_for(self->first, self->second,
                                  other.first, other.second));
        self->first  = other.first;
        self->second = other.second;
    }

    /* Move ctor: same pattern as copy. */
    pair(pair&& other) noexcept : first(), second() {
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
