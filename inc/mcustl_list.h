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
 * @file mcustl_list.h
 * @brief mcustl::list<T> - Doubly-linked list for embedded systems
 *
 * Uses a pool-based approach: all nodes are stored in a single contiguous
 * dalloc allocation. Nodes link via indices (not pointers) because dalloc
 * defragmentation moves memory and only updates the registered base pointer.
 *
 * O(1) insert/erase at any position via iterator (amortized for pool growth).
 * Bidirectional iteration. Iterators remain valid after insert/erase/growth.
 */

#ifndef MCUSTL_LIST_H
#define MCUSTL_LIST_H

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

#if MCUSTL_USE_LIST

#include <new>
#include <type_traits>

namespace mcustl {

template <typename T>
class list
{
public:
    static constexpr uint32_t NPOS = 0xFFFFFFFF;

private:
    struct Node {
        T data;
        uint32_t prev;
        uint32_t next;
    };

    Node *nodes_;
    Node *nodes_res_;
    uint32_t size_;
    uint32_t capacity_;
    uint32_t head_;
    uint32_t tail_;
    uint32_t free_head_;
    heap_t *alloc_mem_ptr;

    bool reserve_new_memory(uint32_t new_cap, Node **dest) {
#ifdef USE_SINGLE_HEAP_MEMORY
        if (alloc_mem_ptr == NULL) {
            alloc_mem_ptr = mcustl_get_default_heap();
        }
#endif
        uint32_t alloc_size = sizeof(Node) * new_cap;
        dalloc(alloc_mem_ptr, alloc_size, reinterpret_cast<void**>(dest));
        return *dest != NULL;
    }

    Node* node_at(uint32_t idx) const {
        return (Node*)((size_t)nodes_ + idx * sizeof(Node));
    }

    Node* node_at_buf(Node* buf, uint32_t idx) const {
        return (Node*)((size_t)buf + idx * sizeof(Node));
    }

    uint32_t alloc_node() {
        if (free_head_ != NPOS) {
            uint32_t idx = free_head_;
            free_head_ = node_at(idx)->next;
            return idx;
        }
        return NPOS;
    }

    void free_node(uint32_t idx) {
        node_at(idx)->data.~T();
        new (&node_at(idx)->data) T;
        node_at(idx)->prev = NPOS;
        node_at(idx)->next = free_head_;
        free_head_ = idx;
    }

    bool grow() {
        uint32_t new_cap;
        if (capacity_ == 0) {
            new_cap = 4;
        } else {
            new_cap = static_cast<uint32_t>(
                static_cast<float>(capacity_) * MCUSTL_LIST_CAPACITY_RESERVE_KOEF);
            if (new_cap <= capacity_) new_cap = capacity_ + 1;
        }
        return reserve(new_cap);
    }

    void unlink_node(uint32_t idx) {
        Node* n = node_at(idx);
        if (n->prev != NPOS) {
            node_at(n->prev)->next = n->next;
        } else {
            head_ = n->next;
        }
        if (n->next != NPOS) {
            node_at(n->next)->prev = n->prev;
        } else {
            tail_ = n->prev;
        }
    }

    void link_node_before(uint32_t new_idx, uint32_t pos_idx) {
        Node* n = node_at(new_idx);
        if (pos_idx == NPOS) {
            n->prev = tail_;
            n->next = NPOS;
            if (tail_ != NPOS) {
                node_at(tail_)->next = new_idx;
            } else {
                head_ = new_idx;
            }
            tail_ = new_idx;
        } else {
            uint32_t prev_idx = node_at(pos_idx)->prev;
            n->prev = prev_idx;
            n->next = pos_idx;
            node_at(pos_idx)->prev = new_idx;
            if (prev_idx != NPOS) {
                node_at(prev_idx)->next = new_idx;
            } else {
                head_ = new_idx;
            }
        }
    }

public:
    class const_iterator;

    class iterator {
        friend class list;
        friend class const_iterator;
        list* owner_;
        uint32_t idx_;
    public:
        iterator() : owner_(nullptr), idx_(NPOS) {}
        iterator(list* owner, uint32_t idx) : owner_(owner), idx_(idx) {}

        T& operator*() const { return owner_->node_at(idx_)->data; }
        T* operator->() const { return &owner_->node_at(idx_)->data; }

        iterator& operator++() {
            idx_ = owner_->node_at(idx_)->next;
            return *this;
        }
        iterator& operator--() {
            if (idx_ == NPOS) idx_ = owner_->tail_;
            else idx_ = owner_->node_at(idx_)->prev;
            return *this;
        }

        bool operator==(const iterator& other) const { return idx_ == other.idx_; }
        bool operator!=(const iterator& other) const { return idx_ != other.idx_; }
    };

    class const_iterator {
        friend class list;
        const list* owner_;
        uint32_t idx_;
    public:
        const_iterator() : owner_(nullptr), idx_(NPOS) {}
        const_iterator(const list* owner, uint32_t idx) : owner_(owner), idx_(idx) {}
        const_iterator(const iterator& it) : owner_(it.owner_), idx_(it.idx_) {}

        const T& operator*() const { return owner_->node_at(idx_)->data; }
        const T* operator->() const { return &owner_->node_at(idx_)->data; }

        const_iterator& operator++() {
            idx_ = owner_->node_at(idx_)->next;
            return *this;
        }
        const_iterator& operator--() {
            if (idx_ == NPOS) idx_ = owner_->tail_;
            else idx_ = owner_->node_at(idx_)->prev;
            return *this;
        }

        bool operator==(const const_iterator& other) const { return idx_ == other.idx_; }
        bool operator!=(const const_iterator& other) const { return idx_ != other.idx_; }
    };

    iterator begin() { return iterator(this, head_); }
    iterator end() { return iterator(this, NPOS); }
    const_iterator begin() const { return const_iterator(this, head_); }
    const_iterator end() const { return const_iterator(this, NPOS); }

#ifdef USE_SINGLE_HEAP_MEMORY
    list() : nodes_(NULL), nodes_res_(NULL), size_(0), capacity_(0),
             head_(NPOS), tail_(NPOS), free_head_(NPOS),
             alloc_mem_ptr(mcustl_get_default_heap()) {}
#else
    list() : nodes_(NULL), nodes_res_(NULL), size_(0), capacity_(0),
             head_(NPOS), tail_(NPOS), free_head_(NPOS),
             alloc_mem_ptr(NULL) {}

    list(heap_t* heap) : nodes_(NULL), nodes_res_(NULL), size_(0), capacity_(0),
             head_(NPOS), tail_(NPOS), free_head_(NPOS),
             alloc_mem_ptr(heap) {}
#endif

    list(const list& other);
    list(list&& other) noexcept;
    ~list();

    list<T>& operator=(const list& other);
    list<T>& operator=(list&& other) noexcept;

    T& front();
    T& back();
    const T& front() const;
    const T& back() const;

    bool empty() const { return size_ == 0; }
    uint32_t size() const { return size_; }

    bool reserve(uint32_t new_cap);
    void clear();

    bool push_back(const T& val);
    bool push_back(T&& val);
    bool push_front(const T& val);
    bool push_front(T&& val);
    bool pop_back();
    bool pop_front();

    iterator insert(iterator pos, const T& val);
    iterator insert(iterator pos, T&& val);
    iterator erase(iterator pos);

    bool remove(const T& val);
    uint32_t remove_all(const T& val);
};

/* ==================== Implementation ==================== */

template<typename T>
T& list<T>::front() {
    return node_at(head_)->data;
}

template<typename T>
T& list<T>::back() {
    return node_at(tail_)->data;
}

template<typename T>
const T& list<T>::front() const {
    return node_at(head_)->data;
}

template<typename T>
const T& list<T>::back() const {
    return node_at(tail_)->data;
}

template<typename T>
bool list<T>::reserve(uint32_t new_cap) {
    if (capacity_ >= new_cap) {
        return true;
    }

    heap_lock(alloc_mem_ptr);

    if (reserve_new_memory(new_cap, &nodes_res_)) {
        // Placement-new all nodes in new buffer
        for (uint32_t i = 0; i < new_cap; i++) {
            new (&node_at_buf(nodes_res_, i)->data) T;
            node_at_buf(nodes_res_, i)->prev = NPOS;
            node_at_buf(nodes_res_, i)->next = NPOS;
        }

        // Move existing nodes
        for (uint32_t i = 0; i < capacity_; i++) {
            Node* old_node = node_at(i);
            Node* new_node = node_at_buf(nodes_res_, i);
            new_node->data = static_cast<T&&>(old_node->data);
            new_node->prev = old_node->prev;
            new_node->next = old_node->next;
        }

        // Destruct old nodes
        for (uint32_t i = 0; i < capacity_; i++) {
            node_at(i)->data.~T();
        }

        // Free old buffer
        if (validate_ptr(alloc_mem_ptr, reinterpret_cast<void**>(&nodes_), USING_PTR_ADDRESS, NULL) != false) {
            dfree(alloc_mem_ptr, reinterpret_cast<void**>(&nodes_), USING_PTR_ADDRESS);
        }

        replace_pointers(alloc_mem_ptr, reinterpret_cast<void**>(&nodes_res_), reinterpret_cast<void**>(&nodes_));

        // Build free list from new slots
        uint32_t old_free = free_head_;
        free_head_ = old_free;  // preserve existing free chain

        // Chain new slots into free list
        for (uint32_t i = new_cap - 1; i >= capacity_; i--) {
            node_at(i)->prev = NPOS;
            node_at(i)->next = free_head_;
            free_head_ = i;
            if (i == 0) break;  // prevent underflow on uint32_t
        }

        capacity_ = new_cap;
        heap_unlock(alloc_mem_ptr);
        return true;
    }

    heap_unlock(alloc_mem_ptr);
    return false;
}

template<typename T>
void list<T>::clear() {
    heap_lock(alloc_mem_ptr);

    // Walk active list and destruct+reinit data
    uint32_t cur = head_;
    while (cur != NPOS) {
        uint32_t next = node_at(cur)->next;
        node_at(cur)->data.~T();
        new (&node_at(cur)->data) T;
        cur = next;
    }

    // Rebuild free list from all slots
    head_ = NPOS;
    tail_ = NPOS;
    size_ = 0;
    free_head_ = NPOS;
    for (uint32_t i = 0; i < capacity_; i++) {
        node_at(i)->prev = NPOS;
        node_at(i)->next = free_head_;
        free_head_ = i;
    }

    heap_unlock(alloc_mem_ptr);
}

template<typename T>
bool list<T>::push_back(const T& val) {
    heap_lock(alloc_mem_ptr);

    if (free_head_ == NPOS) {
        if (!grow()) {
            heap_unlock(alloc_mem_ptr);
            return false;
        }
    }

    uint32_t idx = alloc_node();
    Node* n = node_at(idx);
    n->data = val;
    n->prev = tail_;
    n->next = NPOS;

    if (tail_ != NPOS) {
        node_at(tail_)->next = idx;
    } else {
        head_ = idx;
    }
    tail_ = idx;
    size_++;

    heap_unlock(alloc_mem_ptr);
    return true;
}

template<typename T>
bool list<T>::push_back(T&& val) {
    heap_lock(alloc_mem_ptr);

    if (free_head_ == NPOS) {
        if (!grow()) {
            heap_unlock(alloc_mem_ptr);
            return false;
        }
    }

    uint32_t idx = alloc_node();
    Node* n = node_at(idx);
    n->data = static_cast<T&&>(val);
    n->prev = tail_;
    n->next = NPOS;

    if (tail_ != NPOS) {
        node_at(tail_)->next = idx;
    } else {
        head_ = idx;
    }
    tail_ = idx;
    size_++;

    heap_unlock(alloc_mem_ptr);
    return true;
}

template<typename T>
bool list<T>::push_front(const T& val) {
    heap_lock(alloc_mem_ptr);

    if (free_head_ == NPOS) {
        if (!grow()) {
            heap_unlock(alloc_mem_ptr);
            return false;
        }
    }

    uint32_t idx = alloc_node();
    Node* n = node_at(idx);
    n->data = val;
    n->prev = NPOS;
    n->next = head_;

    if (head_ != NPOS) {
        node_at(head_)->prev = idx;
    } else {
        tail_ = idx;
    }
    head_ = idx;
    size_++;

    heap_unlock(alloc_mem_ptr);
    return true;
}

template<typename T>
bool list<T>::push_front(T&& val) {
    heap_lock(alloc_mem_ptr);

    if (free_head_ == NPOS) {
        if (!grow()) {
            heap_unlock(alloc_mem_ptr);
            return false;
        }
    }

    uint32_t idx = alloc_node();
    Node* n = node_at(idx);
    n->data = static_cast<T&&>(val);
    n->prev = NPOS;
    n->next = head_;

    if (head_ != NPOS) {
        node_at(head_)->prev = idx;
    } else {
        tail_ = idx;
    }
    head_ = idx;
    size_++;

    heap_unlock(alloc_mem_ptr);
    return true;
}

template<typename T>
typename list<T>::iterator list<T>::insert(iterator pos, const T& val) {
    heap_lock(alloc_mem_ptr);

    if (free_head_ == NPOS) {
        if (!grow()) {
            heap_unlock(alloc_mem_ptr);
            return end();
        }
    }

    uint32_t idx = alloc_node();
    node_at(idx)->data = val;
    link_node_before(idx, pos.idx_);
    size_++;

    heap_unlock(alloc_mem_ptr);
    return iterator(this, idx);
}

template<typename T>
typename list<T>::iterator list<T>::insert(iterator pos, T&& val) {
    heap_lock(alloc_mem_ptr);

    if (free_head_ == NPOS) {
        if (!grow()) {
            heap_unlock(alloc_mem_ptr);
            return end();
        }
    }

    uint32_t idx = alloc_node();
    node_at(idx)->data = static_cast<T&&>(val);
    link_node_before(idx, pos.idx_);
    size_++;

    heap_unlock(alloc_mem_ptr);
    return iterator(this, idx);
}

template<typename T>
typename list<T>::iterator list<T>::erase(iterator pos) {
    if (pos.idx_ == NPOS) return end();

    heap_lock(alloc_mem_ptr);

    uint32_t next_idx = node_at(pos.idx_)->next;
    unlink_node(pos.idx_);
    free_node(pos.idx_);
    size_--;

    heap_unlock(alloc_mem_ptr);
    return iterator(this, next_idx);
}

template<typename T>
bool list<T>::pop_back() {
    if (size_ == 0) return false;

    heap_lock(alloc_mem_ptr);

    uint32_t idx = tail_;
    unlink_node(idx);
    free_node(idx);
    size_--;

    heap_unlock(alloc_mem_ptr);
    return true;
}

template<typename T>
bool list<T>::pop_front() {
    if (size_ == 0) return false;

    heap_lock(alloc_mem_ptr);

    uint32_t idx = head_;
    unlink_node(idx);
    free_node(idx);
    size_--;

    heap_unlock(alloc_mem_ptr);
    return true;
}

template<typename T>
bool list<T>::remove(const T& val) {
    heap_lock(alloc_mem_ptr);

    uint32_t cur = head_;
    while (cur != NPOS) {
        if (node_at(cur)->data == val) {
            unlink_node(cur);
            free_node(cur);
            size_--;
            heap_unlock(alloc_mem_ptr);
            return true;
        }
        cur = node_at(cur)->next;
    }

    heap_unlock(alloc_mem_ptr);
    return false;
}

template<typename T>
uint32_t list<T>::remove_all(const T& val) {
    heap_lock(alloc_mem_ptr);

    uint32_t removed = 0;
    uint32_t cur = head_;
    while (cur != NPOS) {
        uint32_t next = node_at(cur)->next;
        if (node_at(cur)->data == val) {
            unlink_node(cur);
            free_node(cur);
            size_--;
            removed++;
        }
        cur = next;
    }

    heap_unlock(alloc_mem_ptr);
    return removed;
}

template<typename T>
list<T>::list(const list& other) {
    nodes_ = NULL;
    nodes_res_ = NULL;
    size_ = 0;
    capacity_ = 0;
    head_ = NPOS;
    tail_ = NPOS;
    free_head_ = NPOS;
    alloc_mem_ptr = other.alloc_mem_ptr;

    heap_lock(alloc_mem_ptr);
    uint32_t cur = other.head_;
    while (cur != NPOS) {
        push_back(other.node_at(cur)->data);
        cur = other.node_at(cur)->next;
    }
    heap_unlock(alloc_mem_ptr);
}

template<typename T>
list<T>& list<T>::operator=(const list& other) {
    if (&other != this) {
        heap_lock(alloc_mem_ptr);

        // Destruct active nodes
        if (nodes_ != NULL) {
            for (uint32_t i = 0; i < capacity_; i++) {
                node_at(i)->data.~T();
            }
            dfree(alloc_mem_ptr, reinterpret_cast<void**>(&nodes_), USING_PTR_ADDRESS);
        }

        nodes_ = NULL;
        nodes_res_ = NULL;
        size_ = 0;
        capacity_ = 0;
        head_ = NPOS;
        tail_ = NPOS;
        free_head_ = NPOS;
        alloc_mem_ptr = other.alloc_mem_ptr;

        uint32_t cur = other.head_;
        while (cur != NPOS) {
            push_back(other.node_at(cur)->data);
            cur = other.node_at(cur)->next;
        }
        heap_unlock(alloc_mem_ptr);
    }
    return *this;
}

template<typename T>
list<T>::list(list&& other) noexcept {
    alloc_mem_ptr = other.alloc_mem_ptr;
    size_ = other.size_;
    capacity_ = other.capacity_;
    head_ = other.head_;
    tail_ = other.tail_;
    free_head_ = other.free_head_;
    nodes_ = NULL;
    nodes_res_ = NULL;

    if (other.nodes_ != NULL) {
        heap_lock(alloc_mem_ptr);
        replace_pointers(alloc_mem_ptr, (void**)&other.nodes_, (void**)&nodes_);
        heap_unlock(alloc_mem_ptr);
    }

    other.size_ = 0;
    other.capacity_ = 0;
    other.head_ = NPOS;
    other.tail_ = NPOS;
    other.free_head_ = NPOS;
}

template<typename T>
list<T>& list<T>::operator=(list&& other) noexcept {
    if (this != &other) {
        // Free current data
        if (nodes_ != NULL) {
            heap_lock(alloc_mem_ptr);
            for (uint32_t i = 0; i < capacity_; i++) {
                node_at(i)->data.~T();
            }
            dfree(alloc_mem_ptr, reinterpret_cast<void**>(&nodes_), USING_PTR_ADDRESS);
            heap_unlock(alloc_mem_ptr);
        }

        // Transfer from other
        alloc_mem_ptr = other.alloc_mem_ptr;
        size_ = other.size_;
        capacity_ = other.capacity_;
        head_ = other.head_;
        tail_ = other.tail_;
        free_head_ = other.free_head_;
        nodes_ = NULL;
        nodes_res_ = NULL;

        if (other.nodes_ != NULL) {
            heap_lock(alloc_mem_ptr);
            replace_pointers(alloc_mem_ptr, (void**)&other.nodes_, (void**)&nodes_);
            heap_unlock(alloc_mem_ptr);
        }

        other.size_ = 0;
        other.capacity_ = 0;
        other.head_ = NPOS;
        other.tail_ = NPOS;
        other.free_head_ = NPOS;
    }
    return *this;
}

template<typename T>
list<T>::~list() {
    if (nodes_ != NULL) {
        heap_t* saved_heap = alloc_mem_ptr;
        uint32_t saved_cap = capacity_;

        heap_lock(saved_heap);

        Node* saved_nodes = NULL;
        replace_pointers(saved_heap, (void**)&nodes_, (void**)&saved_nodes);

        for (uint32_t i = 0; i < saved_cap; i++) {
            ((Node*)((size_t)saved_nodes + i * sizeof(Node)))->data.~T();
        }

        dfree(saved_heap, (void**)&saved_nodes, USING_PTR_ADDRESS);

        heap_unlock(saved_heap);
    }
}

} // namespace mcustl

#endif // MCUSTL_USE_LIST

#endif // MCUSTL_LIST_H
