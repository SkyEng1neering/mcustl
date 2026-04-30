/**
 * @file mcustl_json.h
 * @brief mcustl::json — flexible JSON value type, in the spirit of nlohmann::json
 *
 * Construct from any of: null, bool, int64_t, double, const char*,
 * mcustl::string, mcustl::vector<json>, mcustl::map<string, json>. Parse from
 * raw JSON text. Dump back to text (compact or pretty-printed). Manipulate
 * objects/arrays through operator[], at(), push_back(), erase(), iterators.
 *
 * Memory: every json node holds a heap_t* pointer. When non-null the node and
 * all children allocate from that heap (works exactly like the explicit-heap
 * variants of mcustl::vector / mcustl::map / mcustl::smart_ptr). When null,
 * the default mcustl heap is used. Children inserted into an array/object
 * inherit the parent's heap (deep-copy migration if they were on a different
 * heap).
 */

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

#ifndef MCUSTL_JSON_H
#define MCUSTL_JSON_H

#if MCUSTL_USE_JSON

/* Sister headers (mcustl_string / vector / map / alloc) are already included
 * by mcustl.h before this file. */

#include <stdint.h>
#include <stddef.h>

namespace mcustl {

class json {
public:
    /*--------------------------------------------------------------------
     * Public types
     *--------------------------------------------------------------------*/

    enum class value_t : uint8_t {
        null,
        boolean,
        number_int,
        number_float,
        string,
        array,
        object,
    };

    /* Container aliases used in the public API. */
    using string_t = mcustl::string;
    using array_t  = mcustl::vector<json>;
    using object_t = mcustl::map<string_t, json>;

    /*--------------------------------------------------------------------
     * Construction
     *
     * Heap rules:
     *   - default ctor / scalar ctors  → uses the mcustl default heap
     *   - any ctor + heap_t* override  → uses that explicit heap
     *   - children inserted via operator[] / push_back / etc inherit the
     *     parent's heap (deep-copied if they came from elsewhere)
     *--------------------------------------------------------------------*/

    json();                                          /* null, default heap */
    explicit json(heap_t* heap);                     /* null, explicit heap */
    explicit json(value_t t, heap_t* heap = nullptr);

    json(decltype(nullptr));                         /* nullptr literal */
    json(bool b,        heap_t* heap = nullptr);
    json(int v,         heap_t* heap = nullptr);     /* promotes to int64 */
    json(unsigned v,    heap_t* heap = nullptr);
    json(long v,        heap_t* heap = nullptr);
    json(unsigned long v, heap_t* heap = nullptr);
    json(long long v,   heap_t* heap = nullptr);
    json(unsigned long long v, heap_t* heap = nullptr);
#if MCUSTL_JSON_USE_FLOAT
    json(double v,      heap_t* heap = nullptr);
    json(float v,       heap_t* heap = nullptr);
#endif

    json(const char* s,                heap_t* heap = nullptr);
    json(const string_t& s,            heap_t* heap = nullptr);
    json(string_t&& s,                 heap_t* heap = nullptr);

    json(const array_t&  a,            heap_t* heap = nullptr);
    json(array_t&&       a,            heap_t* heap = nullptr);

    json(const object_t& o,            heap_t* heap = nullptr);
    json(object_t&&      o,            heap_t* heap = nullptr);

    /*--------------------------------------------------------------------
     * Copy / move / dtor
     *--------------------------------------------------------------------*/

    json(const json& other);
    json(json&& other) noexcept;
    json& operator=(const json& other);
    json& operator=(json&& other) noexcept;
    ~json();

    /* Deep clone into a (possibly different) heap. */
    json clone(heap_t* heap) const;

    /*--------------------------------------------------------------------
     * Type & state queries
     *--------------------------------------------------------------------*/

    value_t type() const                { return type_; }
    heap_t* heap() const                { return heap_; }

    /* Heap-detection alias used by mcustl::pair's SFINAE machinery so that
     * `pair<K, json>` can locate a usable heap for tracked_this when only
     * the value half is heap-aware. Returns the explicit heap_ if set,
     * otherwise the default heap. */
    heap_t* get_mem_pointer() const     { return effective_heap(); }

    bool is_null()         const { return type_ == value_t::null; }
    bool is_boolean()      const { return type_ == value_t::boolean; }
    bool is_number_int()   const { return type_ == value_t::number_int; }
    bool is_number_float() const { return type_ == value_t::number_float; }
    bool is_number()       const { return is_number_int() || is_number_float(); }
    bool is_string()       const { return type_ == value_t::string; }
    bool is_array()        const { return type_ == value_t::array; }
    bool is_object()       const { return type_ == value_t::object; }

    /* size():
     *   null/bool/numbers → 0
     *   string            → number of chars
     *   array             → element count
     *   object            → number of keys
     */
    size_t size() const;
    bool   empty() const;

    /*--------------------------------------------------------------------
     * Scalar / container value access (panics on type mismatch)
     *--------------------------------------------------------------------*/

    bool                  get_bool()   const;
    int64_t               get_int()    const;          /* truncates from float */
#if MCUSTL_JSON_USE_FLOAT
    double                get_float()  const;          /* coerces from int64 */
#endif

    const string_t&       get_string() const;
    string_t&             get_string();
    const array_t&        get_array()  const;
    array_t&              get_array();
    const object_t&       get_object() const;
    object_t&             get_object();

    /*--------------------------------------------------------------------
     * Object operations
     *--------------------------------------------------------------------*/

    /* Auto-vivification: creates a null entry if the key is missing.
     * Mutates the receiver into an object if it was null. Asserts on other
     * non-null types. */
    json& operator[](const char* key);
    json& operator[](const string_t& key);

    const json& at(const char* key) const;
    json&       at(const char* key);

    bool contains(const char* key) const;
    bool erase(const char* key);

    /*--------------------------------------------------------------------
     * Array operations
     *--------------------------------------------------------------------*/

    /* Mutates a null receiver into an empty array on first push_back. */
    bool push_back(const json& v);
    bool push_back(json&& v);

    json&       operator[](size_t idx);
    const json& operator[](size_t idx) const;
    /* Disambiguates `j[0]` (where 0 is `int`) — picks the array overload
     * over the (const char*) one. */
    json&       operator[](int idx)       { return (*this)[(size_t)idx]; }
    const json& operator[](int idx) const { return (*this)[(size_t)idx]; }
    json&       at(size_t idx);
    const json& at(size_t idx) const;

    bool erase(size_t idx);

    /*--------------------------------------------------------------------
     * Equality
     *--------------------------------------------------------------------*/

    bool operator==(const json& other) const;
    bool operator!=(const json& other) const { return !(*this == other); }

    /*--------------------------------------------------------------------
     * Iteration (array OR object)
     *
     * For array iteration: it->key() returns "" (empty string).
     * For object iteration: it->key() returns the field name.
     * In both cases it->value() / *it returns a json reference.
     *--------------------------------------------------------------------*/

    class iterator {
        friend class json;
        json* parent_;
        size_t idx_;
    public:
        iterator() : parent_(nullptr), idx_(0) {}
        iterator(json* p, size_t i) : parent_(p), idx_(i) {}

        json& operator*()  const;
        json* operator->() const;

        const string_t& key() const;
        json&           value() const;

        iterator& operator++();
        iterator  operator++(int);

        bool operator==(const iterator& o) const { return parent_ == o.parent_ && idx_ == o.idx_; }
        bool operator!=(const iterator& o) const { return !(*this == o); }
    };

    class const_iterator {
        friend class json;
        const json* parent_;
        size_t idx_;
    public:
        const_iterator() : parent_(nullptr), idx_(0) {}
        const_iterator(const json* p, size_t i) : parent_(p), idx_(i) {}

        const json& operator*()  const;
        const json* operator->() const;

        const string_t& key() const;
        const json&     value() const;

        const_iterator& operator++();
        const_iterator  operator++(int);

        bool operator==(const const_iterator& o) const { return parent_ == o.parent_ && idx_ == o.idx_; }
        bool operator!=(const const_iterator& o) const { return !(*this == o); }
    };

    iterator       begin();
    iterator       end();
    const_iterator begin() const;
    const_iterator end()   const;
    const_iterator cbegin() const { return begin(); }
    const_iterator cend()   const { return end(); }

    /*--------------------------------------------------------------------
     * Parsing & serialization
     *--------------------------------------------------------------------*/

#if MCUSTL_USE_JSON_PARSER
    /* Parse JSON text into a json value. On error returns a null json and
     * (optionally) writes diagnostics to `*err_out` and the byte offset to
     * `*pos_out`. The supplied heap is used for all internal allocations. */
    static json parse(const char* text, size_t len,
                      heap_t* heap = nullptr,
                      const char** err_out = nullptr,
                      size_t* pos_out = nullptr);

    static json parse(const char* text,
                      heap_t* heap = nullptr,
                      const char** err_out = nullptr,
                      size_t* pos_out = nullptr);
#endif

#if MCUSTL_USE_JSON_DUMP
    /* Serialize back to text. indent < 0 → compact (no whitespace).
     * indent >= 0 → pretty-print with that many spaces per nesting level. */
    string_t dump(int indent = -1) const;
#endif

private:
    /*--------------------------------------------------------------------
     * Storage — string/array/object live on the heap and we hold them
     * via owning pointers. The pointers themselves are registered with
     * mcustl, so defragmentation updates them in place; on move/copy we
     * use replace_pointers / def_replace_pointers to migrate the
     * registration to the new node's slot.
     *
     * We bracket every (alloc + placement-new + sub-allocations) sequence
     * with def_heap_lock() so the freshly-allocated block isn't compacted
     * while its in-place container constructor is still running.
     *--------------------------------------------------------------------*/
    value_t type_;
    heap_t* heap_;
    union {
        bool      b_;
        int64_t   i_;
        double    f_;
        string_t* s_;
        array_t*  a_;
        object_t* o_;
    };

    /*--------------------------------------------------------------------
     * Internal helpers
     *--------------------------------------------------------------------*/
    heap_t* effective_heap() const;          /* heap_ or default */
    void    init_scalar(value_t t);          /* common header for scalar ctors */
    void    destroy_payload();               /* free string/array/object */
    void    deep_copy_from(const json& src); /* type_/heap_ already set */
    void    take_from(json& src) noexcept;   /* move payload, clear src */

    /* Allocator wrappers: route through this node's heap. */
    void* alloc_raw(size_t size) const;
    void  free_raw(void* p) const;

    /* Container allocations using node's heap. */
    string_t* alloc_string();
    array_t*  alloc_array();
    object_t* alloc_object();

    /* Mutation helpers — turn null into the requested container in place. */
    void ensure_object();
    void ensure_array();

#if MCUSTL_USE_JSON_DUMP
    /* Dump support — recursive write into out. */
    void dump_into(string_t& out, int indent, int depth) const;
#endif

#if MCUSTL_USE_JSON_PARSER
    /* Parser implementation lives in the .cpp; helper struct used there. */
    struct parser;
    friend struct parser;
#endif
};

} /* namespace mcustl */

#endif /* MCUSTL_USE_JSON */

#endif /* MCUSTL_JSON_H */
