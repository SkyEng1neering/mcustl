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
 */

#ifndef MCUSTL_PAIR_H
#define MCUSTL_PAIR_H

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

namespace mcustl {

template<typename A, typename B>
struct pair {
    A first;
    B second;

    pair() : first(), second() {}
    pair(const A& a, const B& b) : first(a), second(b) {}
    pair(const A& a, B&& b) : first(a), second(static_cast<B&&>(b)) {}
    pair(A&& a, B&& b) : first(static_cast<A&&>(a)), second(static_cast<B&&>(b)) {}

    pair(const pair&) = default;
    pair& operator=(const pair&) = default;
    pair(pair&& other) noexcept
        : first(static_cast<A&&>(other.first)), second(static_cast<B&&>(other.second)) {}
    pair& operator=(pair&& other) noexcept {
        if (this != &other) {
            first = static_cast<A&&>(other.first);
            second = static_cast<B&&>(other.second);
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
