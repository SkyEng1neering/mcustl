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
 * @file mcustl_smart_ptr.h
 * @brief mcustl::smart_ptr<T> - Unique ownership smart pointer for dalloc
 *
 * Also provides:
 * - mcustl::smart_ptr<T[]> array specialization
 * - mcustl::observer_ptr<T> non-owning pointer
 * - mcustl::make_smart<T>() factory function
 * - mcustl::make_smart_array<T>(n) factory function
 */

#ifndef MCUSTL_SMART_PTR_H
#define MCUSTL_SMART_PTR_H

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

#if MCUSTL_USE_SMART_PTR

#include <stdint.h>
#include <stddef.h>
#include <new>

namespace mcustl {

namespace detail {

template <bool B, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> { typedef T type; };

template <typename T, typename U>
struct is_same { static const bool value = false; };

template <typename T>
struct is_same<T, T> { static const bool value = true; };

template <typename From, typename To>
struct is_convertible {
private:
    static void test(To);
    template <typename F>
    static char check(decltype(test(static_cast<F>(nullptr)))*);
    template <typename>
    static long check(...);
public:
    static const bool value = sizeof(check<From>(nullptr)) == sizeof(char);
};

} // namespace detail

/* Forward declarations */
template <typename T> class smart_ptr;
template <typename T> class smart_ptr<T[]>;
template <typename T> class observer_ptr;

/*
 * smart_ptr<T> - Unique ownership smart pointer
 *
 * Allocation Methods:
 *   allocate(n)       - Raw memory, NO constructors
 *   create()          - allocate(1) + constructor
 *   create(args...)   - allocate(1) + constructor with arguments
 *   create_array(n)   - allocate(n) + default constructor for each
 *   create_array(n,v) - allocate(n) + copy constructor for each
 */
template <typename T>
class smart_ptr {
    template <typename U> friend class smart_ptr;

private:
    heap_t* heap_;
    T* ptr_;
    uint32_t size_;
    uint32_t constructed_;

public:
    smart_ptr() noexcept
        : heap_(nullptr)
        , ptr_(nullptr)
        , size_(0)
        , constructed_(0)
    {
#ifdef USE_SINGLE_HEAP_MEMORY
        heap_ = mcustl_get_default_heap();
#endif
    }

#ifndef USE_SINGLE_HEAP_MEMORY
    explicit smart_ptr(heap_t* heap) noexcept
        : heap_(heap)
        , ptr_(nullptr)
        , size_(0)
        , constructed_(0)
    {
    }
#endif

    smart_ptr(smart_ptr&& other) noexcept
        : heap_(other.heap_)
        , ptr_(nullptr)
        , size_(other.size_)
        , constructed_(other.constructed_)
    {
        if (other.ptr_ != nullptr) {
            replace_pointers(heap_, (void**)&other.ptr_, (void**)&ptr_);
        }
        other.ptr_ = nullptr;
        other.size_ = 0;
        other.constructed_ = 0;
    }

    template <typename U,
              typename = typename detail::enable_if<
                  detail::is_convertible<U*, T*>::value &&
                  !detail::is_same<U, T>::value
              >::type>
    smart_ptr(smart_ptr<U>&& other) noexcept
        : heap_(other.heap_)
        , ptr_(nullptr)
        , size_(other.size_)
        , constructed_(other.constructed_)
    {
        if (other.ptr_ != nullptr) {
            replace_pointers(heap_, (void**)&other.ptr_, (void**)&ptr_);
        }
        other.ptr_ = nullptr;
        other.size_ = 0;
        other.constructed_ = 0;
    }

    ~smart_ptr() {
        if (ptr_ == nullptr) {
            return;
        }
        heap_lock(heap_);

        T*       saved_ptr         = ptr_;
        heap_t*  saved_heap        = heap_;
        uint32_t saved_constructed = constructed_;

        for (uint32_t i = 0; i < saved_constructed; i++) {
            saved_ptr[i].~T();
        }

        if (saved_heap != nullptr) {
            dfree(saved_heap, reinterpret_cast<void**>(&saved_ptr), USING_PTR_VALUE);
        }

        heap_unlock(saved_heap);
    }

    smart_ptr(const smart_ptr&) = delete;
    smart_ptr& operator=(const smart_ptr&) = delete;

    smart_ptr& operator=(smart_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            heap_ = other.heap_;
            size_ = other.size_;
            constructed_ = other.constructed_;
            if (other.ptr_ != nullptr) {
                replace_pointers(heap_, (void**)&other.ptr_, (void**)&ptr_);
            }
            other.ptr_ = nullptr;
            other.size_ = 0;
            other.constructed_ = 0;
        }
        return *this;
    }

    template <typename U,
              typename = typename detail::enable_if<
                  detail::is_convertible<U*, T*>::value &&
                  !detail::is_same<U, T>::value
              >::type>
    smart_ptr& operator=(smart_ptr<U>&& other) noexcept {
        reset();
        heap_ = other.heap_;
        size_ = other.size_;
        constructed_ = other.constructed_;
        if (other.ptr_ != nullptr) {
            replace_pointers(heap_, (void**)&other.ptr_, (void**)&ptr_);
        }
        other.ptr_ = nullptr;
        other.size_ = 0;
        other.constructed_ = 0;
        return *this;
    }

    smart_ptr& operator=(decltype(nullptr)) noexcept {
        reset();
        return *this;
    }

    T* get() const noexcept { return ptr_; }
    heap_t* heap() const noexcept { return heap_; }
    uint32_t allocated_size() const noexcept { return size_; }
    uint32_t constructed_count() const noexcept { return constructed_; }

    T& operator*() const {
        MCUSTL_SMART_PTR_ASSERT(ptr_ != nullptr, "Dereferencing null smart_ptr");
        return *ptr_;
    }

    T* operator->() const noexcept {
        MCUSTL_SMART_PTR_ASSERT(ptr_ != nullptr, "Arrow operator on null smart_ptr");
        return ptr_;
    }

    T& operator[](uint32_t i) const {
        MCUSTL_SMART_PTR_ASSERT(ptr_ != nullptr, "Subscript on null smart_ptr");
        MCUSTL_SMART_PTR_ASSERT(i < size_, "Subscript out of bounds");
        return ptr_[i];
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    observer_ptr<T> get_observer() const noexcept;

    T* release() noexcept {
        T* p = ptr_;
        ptr_ = nullptr;
        size_ = 0;
        constructed_ = 0;
        return p;
    }

    void reset(T* p = nullptr, uint32_t new_size = 0) noexcept {
        if (ptr_ != nullptr) {
            for (uint32_t i = 0; i < constructed_; i++) {
                ptr_[i].~T();
            }
            if (heap_ != nullptr) {
                dfree(heap_, (void**)&ptr_, USING_PTR_ADDRESS);
            }
        }
        ptr_ = p;
        size_ = new_size;
        constructed_ = 0;
    }

    void swap(smart_ptr& other) noexcept {
        if (this == &other) return;

        heap_t* my_heap = heap_;
        heap_t* other_heap = other.heap_;

        if (ptr_ != nullptr && other.ptr_ != nullptr) {
            T* tmp = nullptr;
            replace_pointers(my_heap, (void**)&ptr_, (void**)&tmp);
            replace_pointers(other_heap, (void**)&other.ptr_, (void**)&ptr_);
            replace_pointers(my_heap, (void**)&tmp, (void**)&other.ptr_);
        } else if (ptr_ != nullptr) {
            replace_pointers(my_heap, (void**)&ptr_, (void**)&other.ptr_);
        } else if (other.ptr_ != nullptr) {
            replace_pointers(other_heap, (void**)&other.ptr_, (void**)&ptr_);
        }

        heap_ = other_heap;
        other.heap_ = my_heap;

        uint32_t tmp_size = size_;
        size_ = other.size_;
        other.size_ = tmp_size;

        uint32_t tmp_constructed = constructed_;
        constructed_ = other.constructed_;
        other.constructed_ = tmp_constructed;
    }

#ifndef USE_SINGLE_HEAP_MEMORY
    void set_heap(heap_t* heap) noexcept {
        heap_ = heap;
    }
#endif

    bool allocate(uint32_t n = 1) {
#ifdef USE_SINGLE_HEAP_MEMORY
        if (heap_ == nullptr) {
            heap_ = mcustl_get_default_heap();
        }
#endif
        if (heap_ == nullptr) {
            MCUSTL_SMART_PTR_DEBUG("smart_ptr: heap not set\n");
            return false;
        }
        if (ptr_ != nullptr) {
            MCUSTL_SMART_PTR_DEBUG("smart_ptr: already allocated\n");
            return false;
        }
        if (n == 0) {
            return false;
        }
        if (n > SIZE_MAX / sizeof(T)) {
            MCUSTL_SMART_PTR_DEBUG("smart_ptr: allocation size overflow\n");
            return false;
        }

        dalloc(heap_, n * sizeof(T), (void**)&ptr_);
        if (ptr_ == nullptr) {
            MCUSTL_SMART_PTR_DEBUG("smart_ptr: allocation failed\n");
            return false;
        }
        size_ = n;
        return true;
    }

    bool create() {
        if (!allocate(1)) {
            return false;
        }
        new (ptr_) T();
        constructed_ = 1;
        return true;
    }

    template <typename... Args>
    bool create(Args&&... args) {
        if (!allocate(1)) {
            return false;
        }
        new (ptr_) T(static_cast<Args&&>(args)...);
        constructed_ = 1;
        return true;
    }

    bool create_array(uint32_t n) {
        if (!allocate(n)) {
            return false;
        }
        for (uint32_t i = 0; i < n; i++) {
            new (&ptr_[i]) T();
        }
        constructed_ = n;
        return true;
    }

    bool create_array(uint32_t n, const T& value) {
        if (!allocate(n)) {
            return false;
        }
        for (uint32_t i = 0; i < n; i++) {
            new (&ptr_[i]) T(value);
        }
        constructed_ = n;
        return true;
    }

    void destroy() {
        reset();
    }
};

/*--------------------------------------------------------------------
 * Array Specialization smart_ptr<T[]>
 *--------------------------------------------------------------------*/

template <typename T>
class smart_ptr<T[]> {
private:
    heap_t* heap_;
    T* ptr_;
    uint32_t size_;

public:
    smart_ptr() noexcept
        : heap_(nullptr)
        , ptr_(nullptr)
        , size_(0)
    {
#ifdef USE_SINGLE_HEAP_MEMORY
        heap_ = mcustl_get_default_heap();
#endif
    }

#ifndef USE_SINGLE_HEAP_MEMORY
    explicit smart_ptr(heap_t* heap) noexcept
        : heap_(heap)
        , ptr_(nullptr)
        , size_(0)
    {
    }
#endif

    smart_ptr(smart_ptr&& other) noexcept
        : heap_(other.heap_)
        , ptr_(nullptr)
        , size_(other.size_)
    {
        if (other.ptr_ != nullptr) {
            replace_pointers(heap_, (void**)&other.ptr_, (void**)&ptr_);
        }
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    ~smart_ptr() {
        reset();
    }

    smart_ptr(const smart_ptr&) = delete;
    smart_ptr& operator=(const smart_ptr&) = delete;

    smart_ptr& operator=(smart_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            heap_ = other.heap_;
            size_ = other.size_;
            if (other.ptr_ != nullptr) {
                replace_pointers(heap_, (void**)&other.ptr_, (void**)&ptr_);
            }
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    smart_ptr& operator=(decltype(nullptr)) noexcept {
        reset();
        return *this;
    }

    T* get() const noexcept { return ptr_; }
    heap_t* heap() const noexcept { return heap_; }
    uint32_t allocated_size() const noexcept { return size_; }

    T& operator[](uint32_t i) const {
        MCUSTL_SMART_PTR_ASSERT(ptr_ != nullptr, "Subscript on null smart_ptr<T[]>");
        MCUSTL_SMART_PTR_ASSERT(i < size_, "Array index out of bounds");
        return ptr_[i];
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    T* release() noexcept {
        T* p = ptr_;
        ptr_ = nullptr;
        size_ = 0;
        return p;
    }

    void reset(T* p = nullptr, uint32_t new_size = 0) noexcept {
        if (ptr_ != nullptr && heap_ != nullptr) {
            dfree(heap_, (void**)&ptr_, USING_PTR_ADDRESS);
        }
        ptr_ = p;
        size_ = new_size;
    }

    void swap(smart_ptr& other) noexcept {
        if (this == &other) return;

        heap_t* my_heap = heap_;
        heap_t* other_heap = other.heap_;

        if (ptr_ != nullptr && other.ptr_ != nullptr) {
            T* tmp = nullptr;
            replace_pointers(my_heap, (void**)&ptr_, (void**)&tmp);
            replace_pointers(other_heap, (void**)&other.ptr_, (void**)&ptr_);
            replace_pointers(my_heap, (void**)&tmp, (void**)&other.ptr_);
        } else if (ptr_ != nullptr) {
            replace_pointers(my_heap, (void**)&ptr_, (void**)&other.ptr_);
        } else if (other.ptr_ != nullptr) {
            replace_pointers(other_heap, (void**)&other.ptr_, (void**)&ptr_);
        }

        heap_ = other_heap;
        other.heap_ = my_heap;

        uint32_t tmp_size = size_;
        size_ = other.size_;
        other.size_ = tmp_size;
    }

#ifndef USE_SINGLE_HEAP_MEMORY
    void set_heap(heap_t* heap) noexcept {
        heap_ = heap;
    }
#endif

    bool allocate(uint32_t n) {
#ifdef USE_SINGLE_HEAP_MEMORY
        if (heap_ == nullptr) {
            heap_ = mcustl_get_default_heap();
        }
#endif
        if (heap_ == nullptr) {
            MCUSTL_SMART_PTR_DEBUG("smart_ptr<T[]>: heap not set\n");
            return false;
        }
        if (ptr_ != nullptr) {
            MCUSTL_SMART_PTR_DEBUG("smart_ptr<T[]>: already allocated\n");
            return false;
        }
        if (n == 0) {
            return false;
        }
        if (n > SIZE_MAX / sizeof(T)) {
            MCUSTL_SMART_PTR_DEBUG("smart_ptr<T[]>: allocation size overflow\n");
            return false;
        }

        dalloc(heap_, n * sizeof(T), (void**)&ptr_);
        if (ptr_ == nullptr) {
            MCUSTL_SMART_PTR_DEBUG("smart_ptr<T[]>: allocation failed\n");
            return false;
        }
        size_ = n;
        return true;
    }
};

/*--------------------------------------------------------------------
 * observer_ptr<T> - Non-owning pointer wrapper
 *--------------------------------------------------------------------*/

template <typename T>
class observer_ptr {
private:
    T* ptr_;

public:
    constexpr observer_ptr() noexcept : ptr_(nullptr) {}
    constexpr observer_ptr(decltype(nullptr)) noexcept : ptr_(nullptr) {}
    explicit observer_ptr(T* p) noexcept : ptr_(p) {}
    observer_ptr(const smart_ptr<T>& owner) noexcept : ptr_(owner.get()) {}

    observer_ptr(const observer_ptr&) = default;
    observer_ptr& operator=(const observer_ptr&) = default;
    observer_ptr(observer_ptr&&) = default;
    observer_ptr& operator=(observer_ptr&&) = default;

    T* get() const noexcept { return ptr_; }

    T& operator*() const {
        MCUSTL_SMART_PTR_ASSERT(ptr_ != nullptr, "Dereferencing null observer_ptr");
        return *ptr_;
    }

    T* operator->() const noexcept {
        MCUSTL_SMART_PTR_ASSERT(ptr_ != nullptr, "Arrow operator on null observer_ptr");
        return ptr_;
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    void reset(T* p = nullptr) noexcept { ptr_ = p; }

    T* release() noexcept {
        T* p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    void swap(observer_ptr& other) noexcept {
        T* tmp = ptr_;
        ptr_ = other.ptr_;
        other.ptr_ = tmp;
    }
};

/* observer_ptr comparison operators */
template <typename T>
bool operator==(const observer_ptr<T>& a, const observer_ptr<T>& b) noexcept {
    return a.get() == b.get();
}
template <typename T>
bool operator!=(const observer_ptr<T>& a, const observer_ptr<T>& b) noexcept {
    return a.get() != b.get();
}
template <typename T>
bool operator==(const observer_ptr<T>& p, decltype(nullptr)) noexcept {
    return p.get() == nullptr;
}
template <typename T>
bool operator==(decltype(nullptr), const observer_ptr<T>& p) noexcept {
    return p.get() == nullptr;
}
template <typename T>
bool operator!=(const observer_ptr<T>& p, decltype(nullptr)) noexcept {
    return p.get() != nullptr;
}
template <typename T>
bool operator!=(decltype(nullptr), const observer_ptr<T>& p) noexcept {
    return p.get() != nullptr;
}

template <typename T>
observer_ptr<T> make_observer(T* p) noexcept {
    return observer_ptr<T>(p);
}

/* smart_ptr::get_observer() implementation */
template <typename T>
observer_ptr<T> smart_ptr<T>::get_observer() const noexcept {
    return observer_ptr<T>(ptr_);
}

/* Helper: swap */
template <typename T>
void swap(smart_ptr<T>& a, smart_ptr<T>& b) noexcept {
    a.swap(b);
}
template <typename T>
void swap(observer_ptr<T>& a, observer_ptr<T>& b) noexcept {
    a.swap(b);
}

/* smart_ptr comparison operators */
template <typename T>
bool operator==(const smart_ptr<T>& p, decltype(nullptr)) noexcept {
    return p.get() == nullptr;
}
template <typename T>
bool operator==(decltype(nullptr), const smart_ptr<T>& p) noexcept {
    return p.get() == nullptr;
}
template <typename T>
bool operator!=(const smart_ptr<T>& p, decltype(nullptr)) noexcept {
    return p.get() != nullptr;
}
template <typename T>
bool operator!=(decltype(nullptr), const smart_ptr<T>& p) noexcept {
    return p.get() != nullptr;
}
template <typename T, typename U>
bool operator==(const smart_ptr<T>& a, const smart_ptr<U>& b) noexcept {
    return a.get() == b.get();
}
template <typename T, typename U>
bool operator!=(const smart_ptr<T>& a, const smart_ptr<U>& b) noexcept {
    return a.get() != b.get();
}

/*--------------------------------------------------------------------
 * Factory Functions
 *--------------------------------------------------------------------*/

#ifdef USE_SINGLE_HEAP_MEMORY
template <typename T, typename... Args>
smart_ptr<T> make_smart(Args&&... args) {
    smart_ptr<T> p;
    p.create(static_cast<Args&&>(args)...);
    return p;
}

template <typename T>
smart_ptr<T[]> make_smart_array(uint32_t n) {
    smart_ptr<T[]> p;
    p.allocate(n);
    return p;
}
#else
template <typename T, typename... Args>
smart_ptr<T> make_smart(heap_t* heap, Args&&... args) {
    smart_ptr<T> p(heap);
    p.create(static_cast<Args&&>(args)...);
    return p;
}

template <typename T>
smart_ptr<T[]> make_smart_array(heap_t* heap, uint32_t n) {
    smart_ptr<T[]> p(heap);
    p.allocate(n);
    return p;
}
#endif

} // namespace mcustl

#endif // MCUSTL_USE_SMART_PTR

#endif // MCUSTL_SMART_PTR_H
