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
 * @file mcustl_map.h
 * @brief mcustl::map<K,V> - Sorted associative container for embedded systems
 *
 * Pool-based red-black tree. All nodes live in a single contiguous dalloc
 * allocation and link via indices (not pointers) — indices survive dalloc
 * defragmentation which only updates the registered base pointer.
 *
 * O(log n) insert, erase, find, lower_bound, upper_bound.
 * In-order iteration via bidirectional iterator.
 * Requires operator< on K.
 */

#ifndef MCUSTL_MAP_H
#define MCUSTL_MAP_H

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

#if MCUSTL_USE_MAP

#include <new>
#include <type_traits>

namespace mcustl {

template <typename K, typename V>
class map
{
public:
    using value_type = mcustl::pair<K, V>;

    static constexpr uint32_t NPOS = 0xFFFFFFFF;

private:
    struct Node {
        value_type kv;
        uint32_t parent;
        uint32_t left;
        uint32_t right;
        bool red;
    };

    Node *nodes_;
    Node *nodes_res_;
    uint32_t size_;
    uint32_t capacity_;
    uint32_t root_;
    uint32_t free_head_;
    heap_t *alloc_mem_ptr;

    /* ---- Pool management ---- */

    Node* node_at(uint32_t idx) const {
        return (Node*)((size_t)nodes_ + idx * sizeof(Node));
    }

    Node* node_at_buf(Node* buf, uint32_t idx) const {
        return (Node*)((size_t)buf + idx * sizeof(Node));
    }

    bool reserve_new_memory(uint32_t new_cap, Node** dest) {
#ifdef USE_SINGLE_HEAP_MEMORY
        if (alloc_mem_ptr == NULL) {
            alloc_mem_ptr = mcustl_get_default_heap();
        }
#endif
        uint32_t alloc_size = sizeof(Node) * new_cap;
        dalloc(alloc_mem_ptr, alloc_size, reinterpret_cast<void**>(dest));
        return *dest != NULL;
    }

    uint32_t alloc_node() {
        if (free_head_ != NPOS) {
            uint32_t idx = free_head_;
            free_head_ = node_at(idx)->right;
            return idx;
        }
        return NPOS;
    }

    void free_node(uint32_t idx) {
        node_at(idx)->kv.~value_type();
        new (&node_at(idx)->kv) value_type();
        node_at(idx)->parent = NPOS;
        node_at(idx)->left = NPOS;
        node_at(idx)->right = free_head_;
        node_at(idx)->red = false;
        free_head_ = idx;
    }

    bool grow() {
        uint32_t new_cap;
        if (capacity_ == 0) {
            new_cap = 4;
        } else {
            new_cap = static_cast<uint32_t>(
                static_cast<float>(capacity_) * MCUSTL_MAP_CAPACITY_RESERVE_KOEF);
            if (new_cap <= capacity_) new_cap = capacity_ + 1;
        }
        return reserve(new_cap);
    }

    /* ---- RB tree rotations ---- */

    void left_rotate(uint32_t x) {
        uint32_t y = node_at(x)->right;
        node_at(x)->right = node_at(y)->left;
        if (node_at(y)->left != NPOS) {
            node_at(node_at(y)->left)->parent = x;
        }
        node_at(y)->parent = node_at(x)->parent;
        if (node_at(x)->parent == NPOS) {
            root_ = y;
        } else if (x == node_at(node_at(x)->parent)->left) {
            node_at(node_at(x)->parent)->left = y;
        } else {
            node_at(node_at(x)->parent)->right = y;
        }
        node_at(y)->left = x;
        node_at(x)->parent = y;
    }

    void right_rotate(uint32_t y) {
        uint32_t x = node_at(y)->left;
        node_at(y)->left = node_at(x)->right;
        if (node_at(x)->right != NPOS) {
            node_at(node_at(x)->right)->parent = y;
        }
        node_at(x)->parent = node_at(y)->parent;
        if (node_at(y)->parent == NPOS) {
            root_ = x;
        } else if (y == node_at(node_at(y)->parent)->left) {
            node_at(node_at(y)->parent)->left = x;
        } else {
            node_at(node_at(y)->parent)->right = x;
        }
        node_at(x)->right = y;
        node_at(y)->parent = x;
    }

    /* ---- RB tree helpers ---- */

    void transplant(uint32_t u, uint32_t v) {
        if (node_at(u)->parent == NPOS) {
            root_ = v;
        } else if (u == node_at(node_at(u)->parent)->left) {
            node_at(node_at(u)->parent)->left = v;
        } else {
            node_at(node_at(u)->parent)->right = v;
        }
        if (v != NPOS) {
            node_at(v)->parent = node_at(u)->parent;
        }
    }

    void insert_fixup(uint32_t z) {
        while (z != root_ && node_at(node_at(z)->parent)->red) {
            uint32_t zp = node_at(z)->parent;
            uint32_t zpp = node_at(zp)->parent;

            if (zp == node_at(zpp)->left) {
                uint32_t uncle = node_at(zpp)->right;
                if (uncle != NPOS && node_at(uncle)->red) {
                    node_at(zp)->red = false;
                    node_at(uncle)->red = false;
                    node_at(zpp)->red = true;
                    z = zpp;
                } else {
                    if (z == node_at(zp)->right) {
                        z = zp;
                        left_rotate(z);
                        zp = node_at(z)->parent;
                        zpp = node_at(zp)->parent;
                    }
                    node_at(zp)->red = false;
                    node_at(zpp)->red = true;
                    right_rotate(zpp);
                }
            } else {
                uint32_t uncle = node_at(zpp)->left;
                if (uncle != NPOS && node_at(uncle)->red) {
                    node_at(zp)->red = false;
                    node_at(uncle)->red = false;
                    node_at(zpp)->red = true;
                    z = zpp;
                } else {
                    if (z == node_at(zp)->left) {
                        z = zp;
                        right_rotate(z);
                        zp = node_at(z)->parent;
                        zpp = node_at(zp)->parent;
                    }
                    node_at(zp)->red = false;
                    node_at(zpp)->red = true;
                    left_rotate(zpp);
                }
            }
        }
        node_at(root_)->red = false;
    }

    void erase_fixup(uint32_t x, uint32_t x_parent) {
        while (x != root_ && (x == NPOS || !node_at(x)->red)) {
            if (x == node_at(x_parent)->left) {
                uint32_t w = node_at(x_parent)->right;
                if (node_at(w)->red) {
                    node_at(w)->red = false;
                    node_at(x_parent)->red = true;
                    left_rotate(x_parent);
                    w = node_at(x_parent)->right;
                }
                bool wl_black = (node_at(w)->left == NPOS || !node_at(node_at(w)->left)->red);
                bool wr_black = (node_at(w)->right == NPOS || !node_at(node_at(w)->right)->red);
                if (wl_black && wr_black) {
                    node_at(w)->red = true;
                    x = x_parent;
                    x_parent = node_at(x)->parent;
                } else {
                    if (wr_black) {
                        if (node_at(w)->left != NPOS)
                            node_at(node_at(w)->left)->red = false;
                        node_at(w)->red = true;
                        right_rotate(w);
                        w = node_at(x_parent)->right;
                    }
                    node_at(w)->red = node_at(x_parent)->red;
                    node_at(x_parent)->red = false;
                    if (node_at(w)->right != NPOS)
                        node_at(node_at(w)->right)->red = false;
                    left_rotate(x_parent);
                    x = root_;
                }
            } else {
                uint32_t w = node_at(x_parent)->left;
                if (node_at(w)->red) {
                    node_at(w)->red = false;
                    node_at(x_parent)->red = true;
                    right_rotate(x_parent);
                    w = node_at(x_parent)->left;
                }
                bool wr_black = (node_at(w)->right == NPOS || !node_at(node_at(w)->right)->red);
                bool wl_black = (node_at(w)->left == NPOS || !node_at(node_at(w)->left)->red);
                if (wr_black && wl_black) {
                    node_at(w)->red = true;
                    x = x_parent;
                    x_parent = node_at(x)->parent;
                } else {
                    if (wl_black) {
                        if (node_at(w)->right != NPOS)
                            node_at(node_at(w)->right)->red = false;
                        node_at(w)->red = true;
                        left_rotate(w);
                        w = node_at(x_parent)->left;
                    }
                    node_at(w)->red = node_at(x_parent)->red;
                    node_at(x_parent)->red = false;
                    if (node_at(w)->left != NPOS)
                        node_at(node_at(w)->left)->red = false;
                    right_rotate(x_parent);
                    x = root_;
                }
            }
        }
        if (x != NPOS) node_at(x)->red = false;
    }

    void erase_node(uint32_t z) {
        uint32_t y = z;
        bool y_original_red = node_at(y)->red;
        uint32_t x;
        uint32_t x_parent;

        if (node_at(z)->left == NPOS) {
            x = node_at(z)->right;
            x_parent = node_at(z)->parent;
            transplant(z, node_at(z)->right);
        } else if (node_at(z)->right == NPOS) {
            x = node_at(z)->left;
            x_parent = node_at(z)->parent;
            transplant(z, node_at(z)->left);
        } else {
            y = tree_minimum(node_at(z)->right);
            y_original_red = node_at(y)->red;
            x = node_at(y)->right;

            if (node_at(y)->parent == z) {
                x_parent = y;
            } else {
                x_parent = node_at(y)->parent;
                transplant(y, node_at(y)->right);
                node_at(y)->right = node_at(z)->right;
                node_at(node_at(y)->right)->parent = y;
            }
            transplant(z, y);
            node_at(y)->left = node_at(z)->left;
            node_at(node_at(y)->left)->parent = y;
            node_at(y)->red = node_at(z)->red;
        }

        free_node(z);
        size_--;

        if (!y_original_red) {
            erase_fixup(x, x_parent);
        }
    }

    /* ---- Tree navigation ---- */

    uint32_t tree_minimum(uint32_t x) const {
        if (x == NPOS) return NPOS;
        while (node_at(x)->left != NPOS) x = node_at(x)->left;
        return x;
    }

    uint32_t tree_maximum(uint32_t x) const {
        if (x == NPOS) return NPOS;
        while (node_at(x)->right != NPOS) x = node_at(x)->right;
        return x;
    }

    uint32_t tree_successor(uint32_t x) const {
        if (node_at(x)->right != NPOS)
            return tree_minimum(node_at(x)->right);
        uint32_t y = node_at(x)->parent;
        while (y != NPOS && x == node_at(y)->right) {
            x = y;
            y = node_at(y)->parent;
        }
        return y;
    }

    uint32_t tree_predecessor(uint32_t x) const {
        if (node_at(x)->left != NPOS)
            return tree_maximum(node_at(x)->left);
        uint32_t y = node_at(x)->parent;
        while (y != NPOS && x == node_at(y)->left) {
            x = y;
            y = node_at(y)->parent;
        }
        return y;
    }

    uint32_t tree_find(const K& key) const {
        uint32_t x = root_;
        while (x != NPOS) {
            if (key < node_at(x)->kv.first) {
                x = node_at(x)->left;
            } else if (node_at(x)->kv.first < key) {
                x = node_at(x)->right;
            } else {
                return x;
            }
        }
        return NPOS;
    }

    uint32_t tree_lower_bound(const K& key) const {
        uint32_t result = NPOS;
        uint32_t x = root_;
        while (x != NPOS) {
            if (!(node_at(x)->kv.first < key)) {
                result = x;
                x = node_at(x)->left;
            } else {
                x = node_at(x)->right;
            }
        }
        return result;
    }

    uint32_t tree_upper_bound(const K& key) const {
        uint32_t result = NPOS;
        uint32_t x = root_;
        while (x != NPOS) {
            if (key < node_at(x)->kv.first) {
                result = x;
                x = node_at(x)->left;
            } else {
                x = node_at(x)->right;
            }
        }
        return result;
    }

public:
    /* ---- Iterators ---- */

    class const_iterator;

    class iterator {
        friend class map;
        friend class const_iterator;
        map* owner_;
        uint32_t idx_;
    public:
        iterator() : owner_(nullptr), idx_(NPOS) {}
        iterator(map* owner, uint32_t idx) : owner_(owner), idx_(idx) {}

        value_type& operator*() const { return owner_->node_at(idx_)->kv; }
        value_type* operator->() const { return &owner_->node_at(idx_)->kv; }

        iterator& operator++() {
            idx_ = owner_->tree_successor(idx_);
            return *this;
        }
        iterator& operator--() {
            if (idx_ == NPOS) idx_ = owner_->tree_maximum(owner_->root_);
            else idx_ = owner_->tree_predecessor(idx_);
            return *this;
        }

        bool operator==(const iterator& other) const { return idx_ == other.idx_; }
        bool operator!=(const iterator& other) const { return idx_ != other.idx_; }
    };

    class const_iterator {
        friend class map;
        const map* owner_;
        uint32_t idx_;
    public:
        const_iterator() : owner_(nullptr), idx_(NPOS) {}
        const_iterator(const map* owner, uint32_t idx) : owner_(owner), idx_(idx) {}
        const_iterator(const iterator& it) : owner_(it.owner_), idx_(it.idx_) {}

        const value_type& operator*() const { return owner_->node_at(idx_)->kv; }
        const value_type* operator->() const { return &owner_->node_at(idx_)->kv; }

        const_iterator& operator++() {
            idx_ = owner_->tree_successor(idx_);
            return *this;
        }
        const_iterator& operator--() {
            if (idx_ == NPOS) idx_ = owner_->tree_maximum(owner_->root_);
            else idx_ = owner_->tree_predecessor(idx_);
            return *this;
        }

        bool operator==(const const_iterator& other) const { return idx_ == other.idx_; }
        bool operator!=(const const_iterator& other) const { return idx_ != other.idx_; }
    };

    iterator begin() { return iterator(this, tree_minimum(root_)); }
    iterator end() { return iterator(this, NPOS); }
    const_iterator begin() const { return const_iterator(this, tree_minimum(root_)); }
    const_iterator end() const { return const_iterator(this, NPOS); }

    /* Stale-this-safe traversal helpers. Iterators cache an `owner_` map*
     * pointer that goes stale across defrag; callers that hold a tracked
     * pointer to the map (e.g. json::o_ in the union) can use these to
     * re-fetch through the tracked pointer on each step instead. */
    uint32_t begin_idx() const { return tree_minimum(root_); }
    uint32_t successor_idx(uint32_t idx) const { return tree_successor(idx); }
    value_type& kv_at(uint32_t idx) { return node_at(idx)->kv; }
    const value_type& kv_at(uint32_t idx) const { return node_at(idx)->kv; }

    /* ---- Constructors / destructor ---- */

#ifdef USE_SINGLE_HEAP_MEMORY
    map() : nodes_(NULL), nodes_res_(NULL), size_(0), capacity_(0),
            root_(NPOS), free_head_(NPOS),
            alloc_mem_ptr(mcustl_get_default_heap()) {}
#else
    map() : nodes_(NULL), nodes_res_(NULL), size_(0), capacity_(0),
            root_(NPOS), free_head_(NPOS),
            alloc_mem_ptr(NULL) {}

    map(heap_t* heap) : nodes_(NULL), nodes_res_(NULL), size_(0), capacity_(0),
            root_(NPOS), free_head_(NPOS),
            alloc_mem_ptr(heap) {}
#endif

    map(const map& other);
    map(map&& other) noexcept;
    ~map();

    map& operator=(const map& other);
    map& operator=(map&& other) noexcept;

    /* ---- Capacity ---- */

    uint32_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    bool reserve(uint32_t new_cap);
    void clear();

    /* ---- Modifiers ---- */

    bool insert(const K& key, const V& value);
    bool insert(const K& key, V&& value);
    bool erase(const K& key);
    iterator erase(iterator pos);

    void swap(map& other);

    /* ---- Lookup ---- */

    iterator find(const K& key);
    const_iterator find(const K& key) const;
    uint32_t count(const K& key) const;
    bool contains(const K& key) const;

    iterator lower_bound(const K& key);
    const_iterator lower_bound(const K& key) const;
    iterator upper_bound(const K& key);
    const_iterator upper_bound(const K& key) const;

    V& at(const K& key);
    const V& at(const K& key) const;
    V& operator[](const K& key);
};

/* ==================== Implementation ==================== */

template<typename K, typename V>
bool map<K,V>::reserve(uint32_t new_cap) {
    if (capacity_ >= new_cap) return true;

    /* dfree below may relocate `*this`; route subsequent reads/writes via
     * self so the pseudo-tracker keeps us pointing at the live struct. */
    using SelfT = map<K,V>;
    MCUSTL_TRACKED_THIS(SelfT, this->alloc_mem_ptr);

    heap_lock(self->alloc_mem_ptr);

    if (self->reserve_new_memory(new_cap, &self->nodes_res_)) {
        for (uint32_t i = 0; i < new_cap; i++) {
            new (&self->node_at_buf(self->nodes_res_, i)->kv) value_type();
            self->node_at_buf(self->nodes_res_, i)->parent = NPOS;
            self->node_at_buf(self->nodes_res_, i)->left = NPOS;
            self->node_at_buf(self->nodes_res_, i)->right = NPOS;
            self->node_at_buf(self->nodes_res_, i)->red = false;
        }

        for (uint32_t i = 0; i < self->capacity_; i++) {
            /* Each kv assignment may dfree+defrag and relocate `*this`. The
             * pseudo-tracker on self keeps self valid, but Node* locals
             * computed from self go stale after a defrag. Re-fetch through
             * self before every field assignment so we always write into
             * the freshly-located Node. */
            self->node_at_buf(self->nodes_res_, i)->kv.first =
                self->node_at(i)->kv.first;
            self->node_at_buf(self->nodes_res_, i)->kv.second =
                static_cast<V&&>(self->node_at(i)->kv.second);
            self->node_at_buf(self->nodes_res_, i)->parent = self->node_at(i)->parent;
            self->node_at_buf(self->nodes_res_, i)->left = self->node_at(i)->left;
            self->node_at_buf(self->nodes_res_, i)->right = self->node_at(i)->right;
            self->node_at_buf(self->nodes_res_, i)->red = self->node_at(i)->red;
        }

        for (uint32_t i = 0; i < self->capacity_; i++) {
            /* Destruct first and second separately, re-fetching the node
             * through self between them — A's dtor may dfree+defrag and
             * relocate `*this`, in which case B's compiler-generated
             * destruct path would receive a stale `&kv.second`. */
            self->node_at(i)->kv.first.~K();
            self->node_at(i)->kv.second.~V();
        }

        if (validate_ptr(self->alloc_mem_ptr, reinterpret_cast<void**>(&self->nodes_),
                         USING_PTR_ADDRESS, NULL) != false) {
            dfree(self->alloc_mem_ptr, reinterpret_cast<void**>(&self->nodes_), USING_PTR_ADDRESS);
        }

        replace_pointers(self->alloc_mem_ptr,
                         reinterpret_cast<void**>(&self->nodes_res_),
                         reinterpret_cast<void**>(&self->nodes_));

        for (uint32_t i = new_cap - 1; i >= self->capacity_; i--) {
            self->node_at(i)->parent = NPOS;
            self->node_at(i)->left = NPOS;
            self->node_at(i)->right = self->free_head_;
            self->node_at(i)->red = false;
            self->free_head_ = i;
            if (i == 0) break;
        }

        self->capacity_ = new_cap;
        heap_unlock(self->alloc_mem_ptr);
        return true;
    }

    heap_unlock(self->alloc_mem_ptr);
    return false;
}

template<typename K, typename V>
void map<K,V>::clear() {
    /* kv destructors may dfree (e.g. V=json), which can relocate `*this`. */
    using SelfT = map<K,V>;
    MCUSTL_TRACKED_THIS(SelfT, this->alloc_mem_ptr);
    heap_lock(self->alloc_mem_ptr);

    for (uint32_t i = 0; i < self->capacity_; i++) {
        self->node_at(i)->kv.~value_type();
        new (&self->node_at(i)->kv) value_type();
    }

    self->root_ = NPOS;
    self->size_ = 0;
    self->free_head_ = NPOS;
    for (uint32_t i = 0; i < self->capacity_; i++) {
        self->node_at(i)->parent = NPOS;
        self->node_at(i)->left = NPOS;
        self->node_at(i)->right = self->free_head_;
        self->node_at(i)->red = false;
        self->free_head_ = i;
    }

    heap_unlock(self->alloc_mem_ptr);
}

template<typename K, typename V>
bool map<K,V>::insert(const K& key, const V& value) {
    /* grow() may dfree+defrag and relocate `*this`, plus the kv assignments
     * (which call into K::operator= / V::operator= → string push_back ...)
     * may dfree as they grow inner buffers. Route through self. */
    using SelfT = map<K,V>;
    MCUSTL_TRACKED_THIS(SelfT, this->alloc_mem_ptr);
    heap_lock(self->alloc_mem_ptr);

    uint32_t parent = NPOS;
    uint32_t cur = self->root_;
    while (cur != NPOS) {
        parent = cur;
        if (key < self->node_at(cur)->kv.first) {
            cur = self->node_at(cur)->left;
        } else if (self->node_at(cur)->kv.first < key) {
            cur = self->node_at(cur)->right;
        } else {
            heap_unlock(self->alloc_mem_ptr);
            return false;
        }
    }

    if (self->free_head_ == NPOS) {
        if (!self->grow()) {
            heap_unlock(self->alloc_mem_ptr);
            return false;
        }
    }

    uint32_t z = self->alloc_node();
    self->node_at(z)->kv.first = key;
    self->node_at(z)->kv.second = value;
    self->node_at(z)->parent = parent;
    self->node_at(z)->left = NPOS;
    self->node_at(z)->right = NPOS;
    self->node_at(z)->red = true;

    if (parent == NPOS) {
        self->root_ = z;
    } else if (key < self->node_at(parent)->kv.first) {
        self->node_at(parent)->left = z;
    } else {
        self->node_at(parent)->right = z;
    }

    self->size_++;
    self->insert_fixup(z);

    heap_unlock(self->alloc_mem_ptr);
    return true;
}

template<typename K, typename V>
bool map<K,V>::insert(const K& key, V&& value) {
    /* See insert(const K&, const V&) — same stale-this concerns. */
    using SelfT = map<K,V>;
    MCUSTL_TRACKED_THIS(SelfT, this->alloc_mem_ptr);
    heap_lock(self->alloc_mem_ptr);

    uint32_t parent = NPOS;
    uint32_t cur = self->root_;
    while (cur != NPOS) {
        parent = cur;
        if (key < self->node_at(cur)->kv.first) {
            cur = self->node_at(cur)->left;
        } else if (self->node_at(cur)->kv.first < key) {
            cur = self->node_at(cur)->right;
        } else {
            heap_unlock(self->alloc_mem_ptr);
            return false;
        }
    }

    if (self->free_head_ == NPOS) {
        if (!self->grow()) {
            heap_unlock(self->alloc_mem_ptr);
            return false;
        }
    }

    uint32_t z = self->alloc_node();
    self->node_at(z)->kv.first = key;
    self->node_at(z)->kv.second = static_cast<V&&>(value);
    self->node_at(z)->parent = parent;
    self->node_at(z)->left = NPOS;
    self->node_at(z)->right = NPOS;
    self->node_at(z)->red = true;

    if (parent == NPOS) {
        self->root_ = z;
    } else if (key < self->node_at(parent)->kv.first) {
        self->node_at(parent)->left = z;
    } else {
        self->node_at(parent)->right = z;
    }

    self->size_++;
    self->insert_fixup(z);

    heap_unlock(self->alloc_mem_ptr);
    return true;
}

template<typename K, typename V>
bool map<K,V>::erase(const K& key) {
    /* erase_node→free_node destroys K and V; if either has heap-owned
     * payload (e.g. V=json), its destructor dfrees and may relocate `*this`. */
    using SelfT = map<K,V>;
    MCUSTL_TRACKED_THIS(SelfT, this->alloc_mem_ptr);
    heap_lock(self->alloc_mem_ptr);

    uint32_t z = self->tree_find(key);
    if (z == NPOS) {
        heap_unlock(self->alloc_mem_ptr);
        return false;
    }

    self->erase_node(z);

    heap_unlock(self->alloc_mem_ptr);
    return true;
}

template<typename K, typename V>
typename map<K,V>::iterator map<K,V>::erase(iterator pos) {
    if (pos.idx_ == NPOS) return end();

    using SelfT = map<K,V>;
    MCUSTL_TRACKED_THIS(SelfT, this->alloc_mem_ptr);
    heap_lock(self->alloc_mem_ptr);
    uint32_t next = self->tree_successor(pos.idx_);
    self->erase_node(pos.idx_);
    heap_unlock(self->alloc_mem_ptr);

    return iterator(this, next);
}

template<typename K, typename V>
void map<K,V>::swap(map& other) {
    if (this == &other) return;

    heap_t* ha = alloc_mem_ptr;
    heap_t* hb = other.alloc_mem_ptr;

    heap_lock(ha);
    if (hb != ha) heap_lock(hb);

    Node* tmp_a = nullptr;
    Node* tmp_b = nullptr;

    if (nodes_ != nullptr)
        replace_pointers(ha, (void**)&nodes_, (void**)&tmp_a);
    if (other.nodes_ != nullptr)
        replace_pointers(hb, (void**)&other.nodes_, (void**)&tmp_b);

    if (tmp_a != nullptr)
        replace_pointers(ha, (void**)&tmp_a, (void**)&other.nodes_);
    if (tmp_b != nullptr)
        replace_pointers(hb, (void**)&tmp_b, (void**)&nodes_);

    alloc_mem_ptr = hb;
    other.alloc_mem_ptr = ha;

    uint32_t t;
    t = size_; size_ = other.size_; other.size_ = t;
    t = capacity_; capacity_ = other.capacity_; other.capacity_ = t;
    t = root_; root_ = other.root_; other.root_ = t;
    t = free_head_; free_head_ = other.free_head_; other.free_head_ = t;

    Node* tr = nodes_res_; nodes_res_ = other.nodes_res_; other.nodes_res_ = tr;

    if (hb != ha) heap_unlock(hb);
    heap_unlock(ha);
}

/* ---- Lookup ---- */

template<typename K, typename V>
typename map<K,V>::iterator map<K,V>::find(const K& key) {
    return iterator(this, tree_find(key));
}

template<typename K, typename V>
typename map<K,V>::const_iterator map<K,V>::find(const K& key) const {
    return const_iterator(this, tree_find(key));
}

template<typename K, typename V>
uint32_t map<K,V>::count(const K& key) const {
    return tree_find(key) != NPOS ? 1 : 0;
}

template<typename K, typename V>
bool map<K,V>::contains(const K& key) const {
    return tree_find(key) != NPOS;
}

template<typename K, typename V>
typename map<K,V>::iterator map<K,V>::lower_bound(const K& key) {
    return iterator(this, tree_lower_bound(key));
}

template<typename K, typename V>
typename map<K,V>::const_iterator map<K,V>::lower_bound(const K& key) const {
    return const_iterator(this, tree_lower_bound(key));
}

template<typename K, typename V>
typename map<K,V>::iterator map<K,V>::upper_bound(const K& key) {
    return iterator(this, tree_upper_bound(key));
}

template<typename K, typename V>
typename map<K,V>::const_iterator map<K,V>::upper_bound(const K& key) const {
    return const_iterator(this, tree_upper_bound(key));
}

template<typename K, typename V>
V& map<K,V>::at(const K& key) {
    uint32_t idx = tree_find(key);
    return node_at(idx)->kv.second;
}

template<typename K, typename V>
const V& map<K,V>::at(const K& key) const {
    uint32_t idx = tree_find(key);
    return node_at(idx)->kv.second;
}

template<typename K, typename V>
V& map<K,V>::operator[](const K& key) {
    uint32_t idx = tree_find(key);
    if (idx != NPOS) {
        return node_at(idx)->kv.second;
    }
    insert(key, V());
    idx = tree_find(key);
    return node_at(idx)->kv.second;
}

/* ---- Copy / Move / Destructor ---- */

template<typename K, typename V>
map<K,V>::map(const map& other) {
    nodes_ = NULL;
    nodes_res_ = NULL;
    size_ = 0;
    capacity_ = 0;
    root_ = NPOS;
    free_head_ = NPOS;
    alloc_mem_ptr = other.alloc_mem_ptr;

    /* reserve() and the kv assignments may dfree+defrag and relocate `*this`. */
    using SelfT = map<K,V>;
    MCUSTL_TRACKED_THIS(SelfT, this->alloc_mem_ptr);
    heap_lock(self->alloc_mem_ptr);
    if (other.size_ > 0 && self->reserve(other.capacity_)) {
        for (uint32_t i = 0; i < other.capacity_; i++) {
            Node* src = other.node_at(i);
            Node* dst = self->node_at(i);
            dst->kv = src->kv;
            dst->parent = src->parent;
            dst->left = src->left;
            dst->right = src->right;
            dst->red = src->red;
        }
        self->root_ = other.root_;
        self->size_ = other.size_;
        self->free_head_ = other.free_head_;
    }
    heap_unlock(self->alloc_mem_ptr);
}

template<typename K, typename V>
map<K,V>& map<K,V>::operator=(const map& other) {
    if (this != &other) {
        using SelfT = map<K,V>;
        /* dfree + reserve + kv copy can all dfree+defrag and relocate `*this`.
         * Pin against vect's heap (the heap *this is about to live on). */
        heap_t* track_heap = other.alloc_mem_ptr ? other.alloc_mem_ptr : this->alloc_mem_ptr;
        MCUSTL_TRACKED_THIS(SelfT, track_heap);

        heap_lock(self->alloc_mem_ptr);

        if (self->nodes_ != NULL) {
            for (uint32_t i = 0; i < self->capacity_; i++) {
                self->node_at(i)->kv.~value_type();
            }
            dfree(self->alloc_mem_ptr, reinterpret_cast<void**>(&self->nodes_), USING_PTR_ADDRESS);
        }

        self->nodes_ = NULL;
        self->nodes_res_ = NULL;
        self->size_ = 0;
        self->capacity_ = 0;
        self->root_ = NPOS;
        self->free_head_ = NPOS;
        self->alloc_mem_ptr = other.alloc_mem_ptr;

        if (other.size_ > 0 && self->reserve(other.capacity_)) {
            for (uint32_t i = 0; i < other.capacity_; i++) {
                Node* src = other.node_at(i);
                Node* dst = self->node_at(i);
                dst->kv = src->kv;
                dst->parent = src->parent;
                dst->left = src->left;
                dst->right = src->right;
                dst->red = src->red;
            }
            self->root_ = other.root_;
            self->size_ = other.size_;
            self->free_head_ = other.free_head_;
        }

        heap_unlock(self->alloc_mem_ptr);
    }
    return *this;
}

template<typename K, typename V>
map<K,V>::map(map&& other) noexcept {
    alloc_mem_ptr = other.alloc_mem_ptr;
    size_ = other.size_;
    capacity_ = other.capacity_;
    root_ = other.root_;
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
    other.root_ = NPOS;
    other.free_head_ = NPOS;
}

template<typename K, typename V>
map<K,V>& map<K,V>::operator=(map&& other) noexcept {
    if (this != &other) {
        using SelfT = map<K,V>;
        heap_t* track_heap = this->alloc_mem_ptr ? this->alloc_mem_ptr : other.alloc_mem_ptr;
        MCUSTL_TRACKED_THIS(SelfT, track_heap);

        if (self->nodes_ != NULL) {
            heap_lock(self->alloc_mem_ptr);
            for (uint32_t i = 0; i < self->capacity_; i++) {
                self->node_at(i)->kv.~value_type();
            }
            dfree(self->alloc_mem_ptr, reinterpret_cast<void**>(&self->nodes_), USING_PTR_ADDRESS);
            heap_unlock(self->alloc_mem_ptr);
        }

        self->alloc_mem_ptr = other.alloc_mem_ptr;
        self->size_ = other.size_;
        self->capacity_ = other.capacity_;
        self->root_ = other.root_;
        self->free_head_ = other.free_head_;
        self->nodes_ = NULL;
        self->nodes_res_ = NULL;

        if (other.nodes_ != NULL) {
            heap_lock(self->alloc_mem_ptr);
            replace_pointers(self->alloc_mem_ptr, (void**)&other.nodes_, (void**)&self->nodes_);
            heap_unlock(self->alloc_mem_ptr);
        }

        other.size_ = 0;
        other.capacity_ = 0;
        other.root_ = NPOS;
        other.free_head_ = NPOS;
    }
    return *this;
}

template<typename K, typename V>
map<K,V>::~map() {
    if (nodes_ != NULL) {
        heap_t* saved_heap = alloc_mem_ptr;
        uint32_t saved_cap = capacity_;

        heap_lock(saved_heap);

        Node* saved_nodes = NULL;
        replace_pointers(saved_heap, (void**)&nodes_, (void**)&saved_nodes);

        /* Guard: if the tracker for &nodes_ was already removed by a
         * prior defrag's orphan-removal (e.g. the parent struct holding
         * &this->nodes_ was freed separately and its inner trackers
         * were dropped), `replace_pointers` returns without writing
         * `saved_nodes`. Skip cleanup — the buffer either was already
         * recycled by defrag's orphan path or will be reclaimed when
         * the parent dfree completes. Never dereference NULL. */
        if (saved_nodes != NULL) {
            for (uint32_t i = 0; i < saved_cap; i++) {
                ((Node*)((size_t)saved_nodes + i * sizeof(Node)))->kv.~value_type();
            }

            dfree(saved_heap, (void**)&saved_nodes, USING_PTR_ADDRESS);
        }

        heap_unlock(saved_heap);
    }
}

} // namespace mcustl

#endif // MCUSTL_USE_MAP

#endif // MCUSTL_MAP_H
