/**
 * @file mcustl_json.cpp
 * @brief mcustl::json — implementation.
 */

#include "mcustl.h"

#if MCUSTL_USE_JSON

#include <string.h>

/* Helper: declare a temporary mcustl::string in the active heap.
 * Under USE_SINGLE_HEAP_MEMORY there is only one heap, so the heap pointer
 * has nowhere to go and the default ctor is used. Under multi-heap builds
 * we forward `heap_`. */
#ifdef USE_SINGLE_HEAP_MEMORY
#define MCUSTL_JSON_LOCAL_STRING(name)   string_t name
#else
#define MCUSTL_JSON_LOCAL_STRING(name)   string_t name(heap_)
#endif

namespace mcustl {

/*--------------------------------------------------------------------
 * Internal: heap routing + raw allocations
 *--------------------------------------------------------------------*/

heap_t* json::effective_heap() const {
    return heap_ ? heap_ : mcustl_get_default_heap();
}

/* Allocator-tracked storage:
 *
 * mcustl tracks every live allocation by *the address of the pointer-to-
 * pointer*, so that compaction can update the pointer in place when the
 * underlying memory block moves. That means the address we pass to
 * dalloc/dfree must be stable for the entire lifetime of the allocation.
 *
 * The `s_/a_/o_` members of `json` are union members and share the same
 * storage location, so &s_ === &a_ === &o_ — a stable in-object slot.
 *
 * On move (`take_from`) / heap-crossing assignments, we either deep-copy
 * (which re-allocates and re-registers in the new node) or, for same-heap
 * moves, ask mcustl to update the registered pointer-to-pointer via
 * replace_pointers/def_replace_pointers.
 */

/* alloc_raw/free_raw kept as no-ops on the API surface — every allocation
 * now goes via alloc_string/alloc_array/alloc_object so that the
 * pointer-to-pointer registered with mcustl is &s_/&a_/&o_, not a
 * temporary. */
void* json::alloc_raw(size_t) const { return nullptr; }
void  json::free_raw(void*) const   {}

/* Helpers below run their alloc + placement-new + sub-init sequence inside
 * def_heap_lock so the freshly-allocated block isn't compacted between the
 * dalloc that reserves it and the placement-new that activates it (the
 * container's own ctor may itself call dalloc which can trigger defrag). */

json::string_t* json::alloc_string() {
    def_heap_lock();
#ifdef USE_SINGLE_HEAP_MEMORY
    def_dalloc((uint32_t)sizeof(string_t), reinterpret_cast<void**>(&s_));
#else
    if (heap_) dalloc(heap_, (uint32_t)sizeof(string_t), reinterpret_cast<void**>(&s_));
    else       def_dalloc((uint32_t)sizeof(string_t), reinterpret_cast<void**>(&s_));
#endif
    if (s_) {
#ifdef USE_SINGLE_HEAP_MEMORY
        new (s_) string_t();
#else
        if (heap_) new (s_) string_t(heap_);
        else       new (s_) string_t();
#endif
    }
    def_heap_unlock();
    return s_;
}

json::array_t* json::alloc_array() {
    def_heap_lock();
#ifdef USE_SINGLE_HEAP_MEMORY
    def_dalloc((uint32_t)sizeof(array_t), reinterpret_cast<void**>(&a_));
#else
    if (heap_) dalloc(heap_, (uint32_t)sizeof(array_t), reinterpret_cast<void**>(&a_));
    else       def_dalloc((uint32_t)sizeof(array_t), reinterpret_cast<void**>(&a_));
#endif
    if (a_) {
#ifdef USE_SINGLE_HEAP_MEMORY
        new (a_) array_t();
#else
        if (heap_) new (a_) array_t(heap_);
        else       new (a_) array_t();
#endif
    }
    def_heap_unlock();
    return a_;
}

json::object_t* json::alloc_object() {
    def_heap_lock();
#ifdef USE_SINGLE_HEAP_MEMORY
    def_dalloc((uint32_t)sizeof(object_t), reinterpret_cast<void**>(&o_));
#else
    if (heap_) dalloc(heap_, (uint32_t)sizeof(object_t), reinterpret_cast<void**>(&o_));
    else       def_dalloc((uint32_t)sizeof(object_t), reinterpret_cast<void**>(&o_));
#endif
    if (o_) {
#ifdef USE_SINGLE_HEAP_MEMORY
        new (o_) object_t();
#else
        if (heap_) new (o_) object_t(heap_);
        else       new (o_) object_t();
#endif
    }
    def_heap_unlock();
    return o_;
}

void json::init_scalar(value_t t) {
    type_ = t;
    /* heap_ already set by caller */
    switch (t) {
        case value_t::null:         break;
        case value_t::boolean:      b_ = false; break;
        case value_t::number_int:   i_ = 0; break;
        case value_t::number_float: f_ = 0.0; break;
        case value_t::string:       s_ = nullptr; break;
        case value_t::array:        a_ = nullptr; break;
        case value_t::object:       o_ = nullptr; break;
    }
}

void json::destroy_payload() {
    /*
     * Why this looks the way it does:
     *
     * When `this` itself lives inside a heap-allocated, tracked container
     * (for example a `vector<json>` slot or a `map<string, json>` node),
     * any allocator-touching call inside the destructor (e.g.
     * `s_->~string_t()` which dfrees the string's char buffer) can trigger
     * defragmentation. Defrag relocates blocks and updates *registered*
     * pointers in place — but the C++ `this` pointer captured at method
     * entry is just an stack-local register copy and does not get updated.
     * After defrag, `this` may be stale, so any subsequent `&this->s_`
     * computation produces the wrong tracker key and the dfree silently
     * fails ("Try to free unexisting pointer"), leaking the payload struct
     * and leaving a dangling tracker entry that later corrupts memory.
     *
     * The fix is to *transplant* the tracker entry from `&this->s_/a_/o_`
     * onto a stack-local pointer (via replace_pointers) BEFORE doing the
     * inner destructor call. The stack-local survives any further defrag,
     * so the matching dfree always succeeds.
     */
    switch (type_) {
        case value_t::string:
            if (s_) {
                string_t* local_s = nullptr;
#ifdef USE_SINGLE_HEAP_MEMORY
                def_replace_pointers(reinterpret_cast<void**>(&s_),
                                     reinterpret_cast<void**>(&local_s));
#else
                if (heap_) replace_pointers(heap_,
                                            reinterpret_cast<void**>(&s_),
                                            reinterpret_cast<void**>(&local_s));
                else       def_replace_pointers(reinterpret_cast<void**>(&s_),
                                                reinterpret_cast<void**>(&local_s));
#endif
                local_s->~string_t();
#ifdef USE_SINGLE_HEAP_MEMORY
                def_dfree(reinterpret_cast<void**>(&local_s));
#else
                if (heap_) dfree(heap_, reinterpret_cast<void**>(&local_s), USING_PTR_ADDRESS);
                else       def_dfree(reinterpret_cast<void**>(&local_s));
#endif
            }
            break;
        case value_t::array:
            if (a_) {
                array_t* local_a = nullptr;
#ifdef USE_SINGLE_HEAP_MEMORY
                def_replace_pointers(reinterpret_cast<void**>(&a_),
                                     reinterpret_cast<void**>(&local_a));
#else
                if (heap_) replace_pointers(heap_,
                                            reinterpret_cast<void**>(&a_),
                                            reinterpret_cast<void**>(&local_a));
                else       def_replace_pointers(reinterpret_cast<void**>(&a_),
                                                reinterpret_cast<void**>(&local_a));
#endif
                local_a->~array_t();
#ifdef USE_SINGLE_HEAP_MEMORY
                def_dfree(reinterpret_cast<void**>(&local_a));
#else
                if (heap_) dfree(heap_, reinterpret_cast<void**>(&local_a), USING_PTR_ADDRESS);
                else       def_dfree(reinterpret_cast<void**>(&local_a));
#endif
            }
            break;
        case value_t::object:
            if (o_) {
                object_t* local_o = nullptr;
#ifdef USE_SINGLE_HEAP_MEMORY
                def_replace_pointers(reinterpret_cast<void**>(&o_),
                                     reinterpret_cast<void**>(&local_o));
#else
                if (heap_) replace_pointers(heap_,
                                            reinterpret_cast<void**>(&o_),
                                            reinterpret_cast<void**>(&local_o));
                else       def_replace_pointers(reinterpret_cast<void**>(&o_),
                                                reinterpret_cast<void**>(&local_o));
#endif
                local_o->~object_t();
#ifdef USE_SINGLE_HEAP_MEMORY
                def_dfree(reinterpret_cast<void**>(&local_o));
#else
                if (heap_) dfree(heap_, reinterpret_cast<void**>(&local_o), USING_PTR_ADDRESS);
                else       def_dfree(reinterpret_cast<void**>(&local_o));
#endif
            }
            break;
        default:
            break;
    }
    type_ = value_t::null;
}

/*--------------------------------------------------------------------
 * Construction (scalars)
 *--------------------------------------------------------------------*/

json::json()              : type_(value_t::null), heap_(nullptr) {}
json::json(heap_t* heap)  : type_(value_t::null), heap_(heap)    {}

json::json(value_t t, heap_t* heap) : type_(value_t::null), heap_(heap) {
    switch (t) {
        case value_t::null:
            break;
        case value_t::boolean:      type_ = t; b_ = false; break;
        case value_t::number_int:   type_ = t; i_ = 0;     break;
        case value_t::number_float: type_ = t; f_ = 0.0;   break;
        case value_t::string:       type_ = t; s_ = alloc_string(); break;
        case value_t::array:        type_ = t; a_ = alloc_array();  break;
        case value_t::object:       type_ = t; o_ = alloc_object(); break;
    }
}

json::json(decltype(nullptr))                : type_(value_t::null),         heap_(nullptr) {}
json::json(bool b, heap_t* heap)             : type_(value_t::boolean),      heap_(heap) { b_ = b; }
json::json(int v, heap_t* heap)              : type_(value_t::number_int),   heap_(heap) { i_ = v; }
json::json(unsigned v, heap_t* heap)         : type_(value_t::number_int),   heap_(heap) { i_ = v; }
json::json(long v, heap_t* heap)             : type_(value_t::number_int),   heap_(heap) { i_ = v; }
json::json(unsigned long v, heap_t* heap)    : type_(value_t::number_int),   heap_(heap) { i_ = (int64_t)v; }
json::json(long long v, heap_t* heap)        : type_(value_t::number_int),   heap_(heap) { i_ = v; }
json::json(unsigned long long v, heap_t* heap): type_(value_t::number_int),  heap_(heap) { i_ = (int64_t)v; }
#if MCUSTL_JSON_USE_FLOAT
json::json(double v, heap_t* heap)           : type_(value_t::number_float), heap_(heap) { f_ = v; }
json::json(float v, heap_t* heap)            : type_(value_t::number_float), heap_(heap) { f_ = (double)v; }
#endif

/*--------------------------------------------------------------------
 * String ctors
 *--------------------------------------------------------------------*/

json::json(const char* s, heap_t* heap)
    : type_(value_t::null), heap_(heap)
{
    s_ = alloc_string();
    if (s_) {
        if (s) s_->append(s);
        type_ = value_t::string;
    }
}

json::json(const string_t& s, heap_t* heap)
    : type_(value_t::null), heap_(heap)
{
    s_ = alloc_string();
    if (s_) {
        s_->append(s);
        type_ = value_t::string;
    }
}

json::json(string_t&& s, heap_t* heap)
    : type_(value_t::null), heap_(heap)
{
    s_ = alloc_string();
    if (s_) {
        /* mcustl::string move-assign would be ideal; fall back to copy. */
        s_->append(s);
        type_ = value_t::string;
    }
}

/*--------------------------------------------------------------------
 * Array ctors
 *--------------------------------------------------------------------*/

json::json(const array_t& a, heap_t* heap)
    : type_(value_t::null), heap_(heap)
{
    a_ = alloc_array();
    if (a_) {
        type_ = value_t::array;
        for (uint32_t i = 0; i < a.size(); ++i) {
            json child = const_cast<array_t&>(a).at(i).clone(heap_);
            a_->push_back(child);
        }
    }
}

json::json(array_t&& a, heap_t* heap)
    : type_(value_t::null), heap_(heap)
{
    a_ = alloc_array();
    if (a_) {
        type_ = value_t::array;
        for (uint32_t i = 0; i < a.size(); ++i) {
            json child = a.at(i).clone(heap_);
            a_->push_back(child);
        }
        a.clear();
    }
}

/*--------------------------------------------------------------------
 * Object ctors
 *--------------------------------------------------------------------*/

json::json(const object_t& o, heap_t* heap)
    : type_(value_t::null), heap_(heap)
{
    o_ = alloc_object();
    if (o_) {
        type_ = value_t::object;
        object_t& src = const_cast<object_t&>(o);
        for (auto it = src.begin(); it != src.end(); ++it) {
            json child = it->second.clone(heap_);
            o_->insert(it->first, child);
        }
    }
}

json::json(object_t&& o, heap_t* heap)
    : type_(value_t::null), heap_(heap)
{
    o_ = alloc_object();
    if (o_) {
        type_ = value_t::object;
        for (auto it = o.begin(); it != o.end(); ++it) {
            json child = it->second.clone(heap_);
            o_->insert(it->first, child);
        }
        o.clear();
    }
}

/*--------------------------------------------------------------------
 * Copy / move / dtor / clone
 *--------------------------------------------------------------------*/

json::json(const json& other)
    : type_(value_t::null), heap_(other.heap_)
{
    deep_copy_from(other);
}

json::json(json&& other) noexcept
    : type_(value_t::null), heap_(other.heap_)
{
    take_from(other);
}

json& json::operator=(const json& other) {
    if (this == &other) return *this;
    destroy_payload();
    /* Keep our own heap (don't migrate to other.heap_); deep-copy from other
     * into our heap. */
    deep_copy_from(other);
    return *this;
}

json& json::operator=(json&& other) noexcept {
    if (this == &other) return *this;
    destroy_payload();
    if (heap_ == other.heap_) {
        take_from(other);
    } else {
        /* Different heap: must deep-copy then drop source. */
        deep_copy_from(other);
        other.destroy_payload();
        other.type_ = value_t::null;
    }
    return *this;
}

json::~json() {
    destroy_payload();
}

void json::deep_copy_from(const json& src) {
    switch (src.type_) {
        case value_t::null:                                            break;
        case value_t::boolean:      type_ = src.type_; b_ = src.b_;   break;
        case value_t::number_int:   type_ = src.type_; i_ = src.i_;   break;
        case value_t::number_float: type_ = src.type_; f_ = src.f_;   break;

        case value_t::string: {
            s_ = alloc_string();
            if (s_) {
                if (src.s_) s_->append(*src.s_);
                type_ = src.type_;
            }
            break;
        }
        case value_t::array: {
            a_ = alloc_array();
            if (a_) {
                type_ = src.type_;
                if (src.a_) {
                    array_t& sa = *src.a_;
                    for (uint32_t i = 0; i < sa.size(); ++i) {
                        json child = const_cast<json&>(sa.at(i)).clone(heap_);
                        a_->push_back(child);
                    }
                }
            }
            break;
        }
        case value_t::object: {
            o_ = alloc_object();
            if (o_) {
                type_ = src.type_;
                if (src.o_) {
                    object_t& so = *src.o_;
                    auto it = so.begin();
                    auto e = so.end();
                    while (it != e) {
                        json child = it->second.clone(heap_);
                        o_->insert(it->first, child);
                        ++it;
                    }
                }
            }
            break;
        }
    }
}

void json::take_from(json& src) noexcept {
    type_ = src.type_;
    switch (src.type_) {
        case value_t::null:                                  break;
        case value_t::boolean:      b_ = src.b_;             break;
        case value_t::number_int:   i_ = src.i_;             break;
        case value_t::number_float: f_ = src.f_;             break;

        /* For owning payload kinds we must tell mcustl that the registered
         * pointer-to-pointer has moved from &src.s_ (etc.) to our slot. */
        case value_t::string:
            s_ = src.s_;
#ifdef USE_SINGLE_HEAP_MEMORY
            def_replace_pointers(reinterpret_cast<void**>(&src.s_),
                                 reinterpret_cast<void**>(&s_));
#else
            if (heap_) replace_pointers(heap_,
                                        reinterpret_cast<void**>(&src.s_),
                                        reinterpret_cast<void**>(&s_));
            else       def_replace_pointers(reinterpret_cast<void**>(&src.s_),
                                            reinterpret_cast<void**>(&s_));
#endif
            break;

        case value_t::array:
            a_ = src.a_;
#ifdef USE_SINGLE_HEAP_MEMORY
            def_replace_pointers(reinterpret_cast<void**>(&src.a_),
                                 reinterpret_cast<void**>(&a_));
#else
            if (heap_) replace_pointers(heap_,
                                        reinterpret_cast<void**>(&src.a_),
                                        reinterpret_cast<void**>(&a_));
            else       def_replace_pointers(reinterpret_cast<void**>(&src.a_),
                                            reinterpret_cast<void**>(&a_));
#endif
            break;

        case value_t::object:
            o_ = src.o_;
#ifdef USE_SINGLE_HEAP_MEMORY
            def_replace_pointers(reinterpret_cast<void**>(&src.o_),
                                 reinterpret_cast<void**>(&o_));
#else
            if (heap_) replace_pointers(heap_,
                                        reinterpret_cast<void**>(&src.o_),
                                        reinterpret_cast<void**>(&o_));
            else       def_replace_pointers(reinterpret_cast<void**>(&src.o_),
                                            reinterpret_cast<void**>(&o_));
#endif
            break;
    }
    src.type_ = value_t::null;
    src.s_ = nullptr;
}

json json::clone(heap_t* heap) const {
    json out(heap);
    out.deep_copy_from(*this);
    return out;
}

/*--------------------------------------------------------------------
 * Size / scalars
 *--------------------------------------------------------------------*/

size_t json::size() const {
    switch (type_) {
        case value_t::string: return s_ ? s_->size() : 0;
        case value_t::array:  return a_ ? a_->size() : 0;
        case value_t::object: return o_ ? o_->size() : 0;
        default:              return 0;
    }
}

bool json::empty() const {
    return size() == 0;
}

bool json::get_bool() const {
    /* Strict — caller must check is_boolean(). */
    return type_ == value_t::boolean ? b_ : false;
}

int64_t json::get_int() const {
    if (type_ == value_t::number_int)   return i_;
#if MCUSTL_JSON_USE_FLOAT
    if (type_ == value_t::number_float) return (int64_t)f_;
#endif
    return 0;
}

#if MCUSTL_JSON_USE_FLOAT
double json::get_float() const {
    if (type_ == value_t::number_float) return f_;
    if (type_ == value_t::number_int)   return (double)i_;
    return 0.0;
}
#endif

const json::string_t& json::get_string() const { return *s_; }
json::string_t&       json::get_string()       { return *s_; }
const json::array_t&  json::get_array()  const { return *a_; }
json::array_t&        json::get_array()        { return *a_; }
const json::object_t& json::get_object() const { return *o_; }
json::object_t&       json::get_object()       { return *o_; }

/*--------------------------------------------------------------------
 * Object operations
 *--------------------------------------------------------------------*/

void json::ensure_object() {
    if (type_ == value_t::null) {
        o_ = alloc_object();
        if (o_) type_ = value_t::object;
    }
}

void json::ensure_array() {
    if (type_ == value_t::null) {
        a_ = alloc_array();
        if (a_) type_ = value_t::array;
    }
}

/*
 * Reference safety with mcustl's defragmenting allocator
 * ------------------------------------------------------
 *
 * Stack-allocated mcustl::string registers a tracking pointer at a stack
 * address; its destructor frees the underlying data block, which triggers
 * defragmentation. Defrag relocates blocks above the freed one and updates
 * every *registered* pointer-to-pointer in place — but a precomputed C++
 * reference stored on the stack is invisible to mcustl, so it becomes
 * dangling.
 *
 * Concretely, this is broken:
 *
 *     {
 *         mcustl::string k; k.append("k");
 *         json& slot = (*o_)[k];   // computes reference into map nodes_
 *         return slot;             // k dtor → defrag → slot is stale
 *     }
 *
 * The fix is to never let a precomputed reference cross a tracked-temporary
 * destruction boundary. We do the lookup/insert inside an inner scope, let
 * the temporary die (and defrag run), then recompute the reference via a
 * manual strcmp scan over the now-stable map.
 */

static json* find_slot_by_cstr(json::object_t& m, const char* key) {
    for (auto it = m.begin(); it != m.end(); ++it) {
        if (strcmp(it->first.c_str(), key) == 0) {
            return &it->second;
        }
    }
    return nullptr;
}

json& json::operator[](const char* key) {
    ensure_object();
    /* Fast path: key already present — manual scan, no tracked temp. */
    if (json* slot = find_slot_by_cstr(*o_, key)) {
        return *slot;
    }
    /* Slow path: insert default null. The string lives only inside this
     * scope; defrag at scope exit is fine because we recompute below. */
    {
        MCUSTL_JSON_LOCAL_STRING(k);
        k.append(key);
        o_->insert(k, json());
    }
    /* Recompute the reference after the dust settles. */
    return *find_slot_by_cstr(*o_, key);
}

json& json::operator[](const string_t& key) {
    return (*this)[key.c_str()];
}

const json& json::at(const char* key) const {
    /* Strict — caller is responsible for ensuring the key exists. */
    return *find_slot_by_cstr(*const_cast<object_t*>(o_), key);
}

json& json::at(const char* key) {
    return *find_slot_by_cstr(*o_, key);
}

bool json::contains(const char* key) const {
    if (type_ != value_t::object || !o_) return false;
    return find_slot_by_cstr(*const_cast<object_t*>(o_), key) != nullptr;
}

bool json::erase(const char* key) {
    if (type_ != value_t::object || !o_) return false;
    if (!find_slot_by_cstr(*o_, key)) return false;
    /* Build the key, erase, let the temp die. */
    MCUSTL_JSON_LOCAL_STRING(k);
    k.append(key);
    return o_->erase(k);
}

/*--------------------------------------------------------------------
 * Array operations
 *--------------------------------------------------------------------*/

bool json::push_back(const json& v) {
    ensure_array();
    if (type_ != value_t::array) return false;
    json child = v.clone(heap_);
    return a_->push_back(child);
}

bool json::push_back(json&& v) {
    ensure_array();
    if (type_ != value_t::array) return false;
    if (v.heap_ == heap_) {
        return a_->push_back(static_cast<json&&>(v));
    }
    json child = v.clone(heap_);
    bool ok = a_->push_back(child);
    v.destroy_payload();
    return ok;
}

json&       json::operator[](size_t idx)       { return a_->at((uint32_t)idx); }
const json& json::operator[](size_t idx) const { return const_cast<array_t*>(a_)->at((uint32_t)idx); }
json&       json::at(size_t idx)               { return a_->at((uint32_t)idx); }
const json& json::at(size_t idx) const         { return const_cast<array_t*>(a_)->at((uint32_t)idx); }

bool json::erase(size_t idx) {
    if (type_ != value_t::array || !a_) return false;
    return a_->pop((uint32_t)idx);
}

/*--------------------------------------------------------------------
 * Equality
 *--------------------------------------------------------------------*/

bool json::operator==(const json& other) const {
    if (type_ != other.type_) {
#if MCUSTL_JSON_USE_FLOAT
        /* Allow int↔float compare with value match. */
        if (is_number() && other.is_number()) {
            return get_float() == other.get_float();
        }
#endif
        return false;
    }
    switch (type_) {
        case value_t::null:         return true;
        case value_t::boolean:      return b_ == other.b_;
        case value_t::number_int:   return i_ == other.i_;
#if MCUSTL_JSON_USE_FLOAT
        case value_t::number_float: return f_ == other.f_;
#else
        case value_t::number_float: return false;
#endif

        case value_t::string: {
            const string_t* x = s_;
            const string_t* y = other.s_;
            if (!x || !y) return x == y;
            if (x->size() != y->size()) return false;
            return memcmp(x->c_str(), y->c_str(), x->size()) == 0;
        }
        case value_t::array: {
            if (!a_ || !other.a_) return a_ == other.a_;
            if (a_->size() != other.a_->size()) return false;
            for (uint32_t i = 0; i < a_->size(); ++i) {
                if (a_->at(i) != const_cast<array_t*>(other.a_)->at(i)) return false;
            }
            return true;
        }
        case value_t::object: {
            if (!o_ || !other.o_) return o_ == other.o_;
            if (o_->size() != other.o_->size()) return false;
            for (auto it = o_->begin(); it != o_->end(); ++it) {
                auto match = const_cast<object_t*>(other.o_)->find(it->first);
                if (match == const_cast<object_t*>(other.o_)->end()) return false;
                if (it->second != match->second) return false;
            }
            return true;
        }
    }
    return false;
}

/*--------------------------------------------------------------------
 * Iterator
 *--------------------------------------------------------------------*/

json& json::iterator::operator*() const {
    if (parent_->type_ == json::value_t::array) return parent_->a_->at((uint32_t)idx_);
    /* object */
    auto it = parent_->o_->begin();
    for (size_t i = 0; i < idx_; ++i) ++it;
    return it->second;
}

json* json::iterator::operator->() const { return &**this; }

const json::string_t& json::iterator::key() const {
    static const string_t empty;
    if (parent_->type_ == json::value_t::array) return empty;
    auto it = parent_->o_->begin();
    for (size_t i = 0; i < idx_; ++i) ++it;
    return it->first;
}

json& json::iterator::value() const { return **this; }

json::iterator& json::iterator::operator++() { ++idx_; return *this; }
json::iterator  json::iterator::operator++(int) { iterator t = *this; ++idx_; return t; }

const json& json::const_iterator::operator*() const {
    if (parent_->type_ == json::value_t::array) {
        return const_cast<json::array_t*>(parent_->a_)->at((uint32_t)idx_);
    }
    auto it = const_cast<json::object_t*>(parent_->o_)->begin();
    for (size_t i = 0; i < idx_; ++i) ++it;
    return it->second;
}

const json* json::const_iterator::operator->() const { return &**this; }

const json::string_t& json::const_iterator::key() const {
    static const string_t empty;
    if (parent_->type_ == json::value_t::array) return empty;
    auto it = const_cast<json::object_t*>(parent_->o_)->begin();
    for (size_t i = 0; i < idx_; ++i) ++it;
    return it->first;
}

const json& json::const_iterator::value() const { return **this; }

json::const_iterator& json::const_iterator::operator++() { ++idx_; return *this; }
json::const_iterator  json::const_iterator::operator++(int) { const_iterator t = *this; ++idx_; return t; }

json::iterator       json::begin()       { return iterator(this, 0); }
json::iterator       json::end()         { return iterator(this, size()); }
json::const_iterator json::begin() const { return const_iterator(this, 0); }
json::const_iterator json::end()   const { return const_iterator(this, size()); }

/*--------------------------------------------------------------------
 * Serializer (dump) — gated by MCUSTL_USE_JSON_DUMP.
 *--------------------------------------------------------------------*/

#if MCUSTL_USE_JSON_DUMP

static void append_indent(json::string_t& out, int spaces) {
    for (int i = 0; i < spaces; ++i) out.append(' ');
}

static void append_hex4(json::string_t& out, unsigned v) {
    static const char* hex = "0123456789abcdef";
    out.append('\\');
    out.append('u');
    out.append(hex[(v >> 12) & 0xF]);
    out.append(hex[(v >> 8)  & 0xF]);
    out.append(hex[(v >> 4)  & 0xF]);
    out.append(hex[ v        & 0xF]);
}

static void append_int64(json::string_t& out, int64_t v) {
    char buf[24];
    int pos = 0;
    bool neg = v < 0;
    uint64_t u;
    if (neg) {
        /* Handle INT64_MIN safely. */
        u = (uint64_t)(-(v + 1)) + 1u;
    } else {
        u = (uint64_t)v;
    }
    if (u == 0) {
        buf[pos++] = '0';
    } else {
        while (u > 0) {
            buf[pos++] = (char)('0' + (u % 10));
            u /= 10;
        }
    }
    if (neg) buf[pos++] = '-';
    while (pos > 0) out.append(buf[--pos]);
}

#if MCUSTL_JSON_USE_FLOAT
static void append_double(json::string_t& out, double v) {
    /* Manual round-trip-friendly formatting without stdio.
     * Strategy: handle special cases (NaN/Inf), sign, integer part, then
     * up to 17 significant digits via repeated multiply + extract. */
    if (v != v) { out.append("null"); return; }              /* NaN → null per JSON */
    if (v == 0.0) { out.append("0.0"); return; }

    if (v < 0) { out.append('-'); v = -v; }

    /* Inf */
    const double inf_threshold = 1e308 * 10.0;
    if (v > inf_threshold || v == inf_threshold) {
        out.append("null"); return;
    }

    /* Determine exponent (base 10). */
    int exp10 = 0;
    double tmp = v;
    if (tmp >= 10.0) {
        while (tmp >= 10.0) { tmp /= 10.0; ++exp10; }
    } else if (tmp < 1.0) {
        while (tmp < 1.0) { tmp *= 10.0; --exp10; }
    }

    /* tmp is now in [1, 10). Extract 17 digits. */
    char digits[20];
    int ndig = 17;
    for (int i = 0; i < ndig; ++i) {
        int d = (int)tmp;
        if (d < 0) d = 0;
        if (d > 9) d = 9;
        digits[i] = (char)('0' + d);
        tmp = (tmp - (double)d) * 10.0;
    }
    /* Round half-up using next-digit (one extra). */
    int next = (int)tmp;
    if (next >= 5) {
        for (int i = ndig - 1; i >= 0; --i) {
            if (digits[i] < '9') { digits[i]++; break; }
            digits[i] = '0';
            if (i == 0) {
                /* Carry past leading digit → shift exponent. */
                for (int j = ndig - 1; j > 0; --j) digits[j] = digits[j - 1];
                digits[0] = '1';
                ++exp10;
            }
        }
    }
    /* Trim trailing zeros (keep at least one digit). */
    while (ndig > 1 && digits[ndig - 1] == '0') --ndig;

    /* Decide between fixed and scientific notation. */
    if (exp10 >= -4 && exp10 < 17) {
        if (exp10 >= 0) {
            int int_digits = exp10 + 1;
            if (int_digits >= ndig) {
                for (int i = 0; i < ndig; ++i) out.append(digits[i]);
                for (int i = ndig; i < int_digits; ++i) out.append('0');
                out.append(".0");
            } else {
                for (int i = 0; i < int_digits; ++i) out.append(digits[i]);
                out.append('.');
                for (int i = int_digits; i < ndig; ++i) out.append(digits[i]);
            }
        } else {
            out.append("0.");
            for (int i = exp10 + 1; i < 0; ++i) out.append('0');
            for (int i = 0; i < ndig; ++i) out.append(digits[i]);
        }
    } else {
        out.append(digits[0]);
        if (ndig > 1) {
            out.append('.');
            for (int i = 1; i < ndig; ++i) out.append(digits[i]);
        }
        out.append('e');
        int e = exp10;
        if (e < 0) { out.append('-'); e = -e; } else { out.append('+'); }
        char ebuf[6];
        int ep = 0;
        if (e == 0) ebuf[ep++] = '0';
        while (e > 0) { ebuf[ep++] = (char)('0' + (e % 10)); e /= 10; }
        if (ep < 2) ebuf[ep++] = '0';     /* JSON exponent: at least 2 digits */
        while (ep > 0) out.append(ebuf[--ep]);
    }
}
#endif /* MCUSTL_JSON_USE_FLOAT */

static void append_quoted(json::string_t& out, const json::string_t& s) {
    out.append('"');
    const char* p = s.c_str();
    uint32_t n = s.size();
    for (uint32_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        switch (c) {
            case '"':  out.append('\\'); out.append('"');  break;
            case '\\': out.append('\\'); out.append('\\'); break;
            case '\b': out.append('\\'); out.append('b');  break;
            case '\f': out.append('\\'); out.append('f');  break;
            case '\n': out.append('\\'); out.append('n');  break;
            case '\r': out.append('\\'); out.append('r');  break;
            case '\t': out.append('\\'); out.append('t');  break;
            default:
                if (c < 0x20) {
                    append_hex4(out, c);
                } else {
                    out.append((char)c);
                }
                break;
        }
    }
    out.append('"');
}

void json::dump_into(string_t& out, int indent, int depth) const {
    switch (type_) {
        case value_t::null:    out.append("null");                   return;
        case value_t::boolean: out.append(b_ ? "true" : "false");    return;
        case value_t::number_int:
            append_int64(out, i_);
            return;
        case value_t::number_float:
#if MCUSTL_JSON_USE_FLOAT
            append_double(out, f_);
#else
            out.append("null");          /* shouldn't appear when float disabled */
#endif
            return;
        case value_t::string:
            if (s_) append_quoted(out, *s_);
            else out.append("\"\"");
            return;

        case value_t::array: {
            if (!a_ || a_->size() == 0) { out.append("[]"); return; }
            out.append('[');
            uint32_t n = a_->size();
            for (uint32_t i = 0; i < n; ++i) {
                if (indent >= 0) {
                    out.append('\n');
                    append_indent(out, indent * (depth + 1));
                }
                a_->at(i).dump_into(out, indent, depth + 1);
                if (i + 1 < n) out.append(',');
            }
            if (indent >= 0) {
                out.append('\n');
                append_indent(out, indent * depth);
            }
            out.append(']');
            return;
        }

        case value_t::object: {
            if (!o_ || o_->size() == 0) { out.append("{}"); return; }
            out.append('{');
            uint32_t n = o_->size();
            uint32_t i = 0;
            for (auto it = o_->begin(); it != o_->end(); ++it, ++i) {
                if (indent >= 0) {
                    out.append('\n');
                    append_indent(out, indent * (depth + 1));
                }
                append_quoted(out, it->first);
                out.append(':');
                if (indent >= 0) out.append(' ');
                it->second.dump_into(out, indent, depth + 1);
                if (i + 1 < n) out.append(',');
            }
            if (indent >= 0) {
                out.append('\n');
                append_indent(out, indent * depth);
            }
            out.append('}');
            return;
        }
    }
}

json::string_t json::dump(int indent) const {
#ifdef USE_SINGLE_HEAP_MEMORY
    string_t out;
#else
    string_t out(heap_);
#endif
    dump_into(out, indent, 0);
    return out;
}

#endif /* MCUSTL_USE_JSON_DUMP */

/*--------------------------------------------------------------------
 * Parser — gated by MCUSTL_USE_JSON_PARSER.
 *--------------------------------------------------------------------*/

#if MCUSTL_USE_JSON_PARSER

struct json::parser {
    const char* p;
    const char* end;
    heap_t*     heap;
    const char* err;
    size_t      err_pos;
    int         depth;          /* current nesting depth, capped by MCUSTL_JSON_MAX_DEPTH */

    parser(const char* s, size_t len, heap_t* h)
        : p(s), end(s + len), heap(h), err(nullptr), err_pos(0), depth(0) {}

    void set_err(const char* m) {
        if (!err) {
            err = m;
            err_pos = (size_t)(p - (end - (end - p)));
        }
    }
    void set_err_at(const char* m, const char* pos) {
        if (!err) {
            err = m;
            err_pos = (size_t)(pos - (end - (end - pos)));
        }
    }

    void skip_ws() {
        while (p < end) {
            char c = *p;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++p;
            else break;
        }
    }

    bool match_keyword(const char* kw, size_t len) {
        if (end - p < (ptrdiff_t)len) return false;
        if (memcmp(p, kw, len) != 0) return false;
        p += len;
        return true;
    }

    bool parse_value(json& out) {
        skip_ws();
        if (p >= end) { set_err("unexpected end of input"); return false; }
        char c = *p;
        switch (c) {
            case '{': return parse_object(out);
            case '[': return parse_array(out);
            case '"': return parse_string_value(out);
            case 't': case 'f': return parse_bool(out);
            case 'n': return parse_null(out);
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
                set_err("unexpected character");
                return false;
        }
    }

    bool parse_null(json& out) {
        if (!match_keyword("null", 4)) { set_err("expected 'null'"); return false; }
        out = json(heap);
        return true;
    }

    bool parse_bool(json& out) {
        if (match_keyword("true", 4))  { out = json(true,  heap); return true; }
        if (match_keyword("false", 5)) { out = json(false, heap); return true; }
        set_err("expected 'true' or 'false'");
        return false;
    }

    /* Hand-rolled number parser — no <stdlib.h>, no libm.
     *
     * Recognizes the JSON RFC 8259 grammar:
     *   number = [ '-' ] int [ frac ] [ exp ]
     *   int    = '0' | digit1-9 *digit
     *   frac   = '.' 1*digit
     *   exp    = ('e'|'E') ['+'|'-'] 1*digit
     *
     * Integer-only numbers up to 18 decimal digits go into int64_t; anything
     * with a fractional or exponent part — or that would overflow int64_t —
     * goes into double.
     */
    bool parse_number(json& out) {
        bool negative = false;
        if (p < end && *p == '-') { negative = true; ++p; }

        const char* int_start = p;
        uint64_t int_part = 0;
        int int_digits = 0;
        bool int_overflow = false;
        while (p < end && *p >= '0' && *p <= '9') {
            unsigned d = (unsigned)(*p - '0');
            if (int_part > ((uint64_t)-1 - d) / 10) {
                int_overflow = true;          /* will fall back to double */
            } else {
                int_part = int_part * 10 + d;
            }
            ++p;
            ++int_digits;
        }
        if (int_digits == 0) { set_err("malformed number"); return false; }
        /* JSON forbids leading zeros (e.g. "012") on multi-digit ints. */
        if (int_digits > 1 && *int_start == '0') {
            set_err("leading zero in number"); return false;
        }

        bool is_float = false;
        uint64_t frac_part = 0;
        int frac_digits = 0;
        if (p < end && *p == '.') {
            is_float = true;
            ++p;
            const char* frac_start = p;
            while (p < end && *p >= '0' && *p <= '9') {
                if (frac_digits < 18) {       /* clamp to ~double precision */
                    frac_part = frac_part * 10 + (uint64_t)(*p - '0');
                    ++frac_digits;
                }
                ++p;
            }
            if (p == frac_start) { set_err("malformed number"); return false; }
        }

        int exp_sign = 1;
        int exp_part = 0;
        if (p < end && (*p == 'e' || *p == 'E')) {
            is_float = true;
            ++p;
            if (p < end && (*p == '+' || *p == '-')) {
                if (*p == '-') exp_sign = -1;
                ++p;
            }
            const char* exp_start = p;
            while (p < end && *p >= '0' && *p <= '9') {
                if (exp_part < 10000) {       /* saturate; |exp| > 308 is Inf anyway */
                    exp_part = exp_part * 10 + (*p - '0');
                }
                ++p;
            }
            if (p == exp_start) { set_err("malformed number"); return false; }
        }

        /* Pure int path. INT64_MAX = 9223372036854775807; fits in 19 digits. */
        if (!is_float && !int_overflow && int_digits <= 18) {
            int64_t v = negative ? -(int64_t)int_part : (int64_t)int_part;
            out = json((long long)v, heap);
            return true;
        }

#if MCUSTL_JSON_USE_FLOAT
        /* Float path. Build value = int_part + frac_part*10^-frac_digits,
         * then apply exponent. */
        double v = (double)int_part;
        if (frac_digits > 0) {
            double scale = 1.0;
            for (int i = 0; i < frac_digits; ++i) scale *= 10.0;
            v += (double)frac_part / scale;
        }
        if (exp_part != 0) {
            double e = 1.0;
            for (int i = 0; i < exp_part; ++i) e *= 10.0;
            v = (exp_sign > 0) ? v * e : v / e;
        }
        if (negative) v = -v;
        out = json(v, heap);
        return true;
#else
        (void)frac_part; (void)frac_digits; (void)exp_sign; (void)exp_part;
        set_err("float parsing disabled (MCUSTL_JSON_USE_FLOAT=0)");
        return false;
#endif
    }

    /* Decode \uXXXX into a UTF-8 sequence appended to s. Handles surrogate
     * pairs (high \uD83D + low \uDE00 → 4-byte UTF-8). */
    bool decode_unicode_escape(string_t& s) {
        if (end - p < 4) { set_err("incomplete \\u escape"); return false; }
        unsigned cp = 0;
        for (int k = 0; k < 4; ++k) {
            char c = p[k];
            unsigned d;
            if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
            else { set_err("invalid hex in \\u escape"); return false; }
            cp = (cp << 4) | d;
        }
        p += 4;

        /* High surrogate? Need a low surrogate to follow as \uXXXX. */
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (end - p < 6 || p[0] != '\\' || p[1] != 'u') {
                set_err("expected low surrogate after high surrogate");
                return false;
            }
            p += 2;
            unsigned low = 0;
            for (int k = 0; k < 4; ++k) {
                char c = p[k];
                unsigned d;
                if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
                else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
                else { set_err("invalid hex in low surrogate"); return false; }
                low = (low << 4) | d;
            }
            p += 4;
            if (low < 0xDC00 || low > 0xDFFF) {
                set_err("invalid low surrogate");
                return false;
            }
            cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
        }

        /* Encode cp as UTF-8 */
        if (cp < 0x80) {
            s.append((char)cp);
        } else if (cp < 0x800) {
            s.append((char)(0xC0 | (cp >> 6)));
            s.append((char)(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            s.append((char)(0xE0 | (cp >> 12)));
            s.append((char)(0x80 | ((cp >> 6) & 0x3F)));
            s.append((char)(0x80 | (cp & 0x3F)));
        } else {
            s.append((char)(0xF0 | (cp >> 18)));
            s.append((char)(0x80 | ((cp >> 12) & 0x3F)));
            s.append((char)(0x80 | ((cp >> 6) & 0x3F)));
            s.append((char)(0x80 | (cp & 0x3F)));
        }
        return true;
    }

    bool parse_raw_string(string_t& s) {
        if (p >= end || *p != '"') { set_err("expected '\"'"); return false; }
        ++p;
        while (p < end) {
            char c = *p;
            if (c == '"') { ++p; return true; }
            if (c == '\\') {
                ++p;
                if (p >= end) { set_err("incomplete escape"); return false; }
                switch (*p) {
                    case '"':  s.append('"');  ++p; break;
                    case '\\': s.append('\\'); ++p; break;
                    case '/':  s.append('/');  ++p; break;
                    case 'b':  s.append('\b'); ++p; break;
                    case 'f':  s.append('\f'); ++p; break;
                    case 'n':  s.append('\n'); ++p; break;
                    case 'r':  s.append('\r'); ++p; break;
                    case 't':  s.append('\t'); ++p; break;
                    case 'u':  ++p; if (!decode_unicode_escape(s)) return false; break;
                    default:   set_err("invalid escape"); return false;
                }
            } else if ((unsigned char)c < 0x20) {
                set_err("control character in string");
                return false;
            } else {
                s.append(c);
                ++p;
            }
        }
        set_err("unterminated string");
        return false;
    }

    bool parse_string_value(json& out) {
        json tmp(value_t::string, heap);
        if (!tmp.s_) { set_err("OOM"); return false; }
        if (!parse_raw_string(*tmp.s_)) return false;
        out = static_cast<json&&>(tmp);
        return true;
    }

    bool parse_array(json& out) {
        if (depth >= MCUSTL_JSON_MAX_DEPTH) { set_err("max nesting depth exceeded"); return false; }
        ++depth;
        ++p; /* consume '[' */
        json arr(value_t::array, heap);
        skip_ws();
        if (p < end && *p == ']') { ++p; out = static_cast<json&&>(arr); --depth; return true; }
        for (;;) {
            json elt;
            elt.heap_ = heap;
            if (!parse_value(elt)) { --depth; return false; }
            arr.a_->push_back(elt);
            skip_ws();
            if (p >= end) { set_err("unterminated array"); --depth; return false; }
            if (*p == ',') { ++p; skip_ws(); continue; }
            if (*p == ']') { ++p; out = static_cast<json&&>(arr); --depth; return true; }
            set_err("expected ',' or ']'");
            --depth;
            return false;
        }
    }

    bool parse_object(json& out) {
        if (depth >= MCUSTL_JSON_MAX_DEPTH) { set_err("max nesting depth exceeded"); return false; }
        ++depth;
        ++p; /* consume '{' */
        json obj(value_t::object, heap);
        skip_ws();
        if (p < end && *p == '}') { ++p; out = static_cast<json&&>(obj); --depth; return true; }
        for (;;) {
            skip_ws();
#ifdef USE_SINGLE_HEAP_MEMORY
            json::string_t key;
#else
            json::string_t key(heap);
#endif
            if (!parse_raw_string(key)) { --depth; return false; }
            skip_ws();
            if (p >= end || *p != ':') { set_err("expected ':'"); --depth; return false; }
            ++p;
            skip_ws();
            json val;
            val.heap_ = heap;
            if (!parse_value(val)) { --depth; return false; }
            obj.o_->insert(key, val);
            skip_ws();
            if (p >= end) { set_err("unterminated object"); --depth; return false; }
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; out = static_cast<json&&>(obj); --depth; return true; }
            set_err("expected ',' or '}'");
            --depth;
            return false;
        }
    }
};

json json::parse(const char* text, size_t len, heap_t* heap,
                 const char** err_out, size_t* pos_out) {
    /* Lock the heap for the duration of parsing — the parser builds
     * temporary mcustl::string keys on the stack which register tracking
     * pointers there; freezing defrag protects them and is also faster. */
    def_heap_lock();
    json out(heap);
    if (!text) {
        if (err_out) *err_out = "null input";
        if (pos_out) *pos_out = 0;
        def_heap_unlock();
        return out;
    }
    parser ps(text, len, heap);
    if (!ps.parse_value(out)) {
        if (err_out) *err_out = ps.err ? ps.err : "parse error";
        if (pos_out) *pos_out = (size_t)(ps.p - text);
        out.destroy_payload();
        def_heap_unlock();
        return out;
    }
    ps.skip_ws();
    if (ps.p != ps.end) {
        if (err_out) *err_out = "trailing content after value";
        if (pos_out) *pos_out = (size_t)(ps.p - text);
        out.destroy_payload();
        def_heap_unlock();
        return out;
    }
    if (err_out) *err_out = nullptr;
    if (pos_out) *pos_out = 0;
    def_heap_unlock();
    return out;
}

json json::parse(const char* text, heap_t* heap,
                 const char** err_out, size_t* pos_out) {
    return parse(text, text ? strlen(text) : 0, heap, err_out, pos_out);
}

#endif /* MCUSTL_USE_JSON_PARSER */

} /* namespace mcustl */

#endif /* MCUSTL_USE_JSON */
