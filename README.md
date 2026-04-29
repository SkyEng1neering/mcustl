# mcustl - MCU Standard Template Library

**Author:** Alexey Vasilenko  
**License:** Apache 2.0  
**Version:** 1.1.0

C++ container library for embedded systems (MCU, RTOS). All containers use the **dalloc** defragmenting memory allocator, which automatically compacts heap memory and updates tracked pointers. No standard library heap (`new`/`delete`/`malloc`) is used.

## Components

| Component | Header | Description |
|---|---|---|
| `mcustl::vector<T>` | mcustl_vector.h | Dynamic array, contiguous storage |
| `mcustl::string` | mcustl_string.h | Dynamic string (backed by `vector<char>`) |
| `mcustl::list<T>` | mcustl_list.h | Doubly-linked list, pool-based, index-linked |
| `mcustl::map<K,V>` | mcustl_map.h | Sorted associative container, pool-based red-black tree |
| `mcustl::json` | mcustl_json.h | nlohmann-style JSON value with parse / dump / object / array |
| `mcustl::smart_ptr<T>` | mcustl_smart_ptr.h | Unique ownership smart pointer |
| `mcustl::observer_ptr<T>` | mcustl_smart_ptr.h | Non-owning pointer wrapper |
| `mcustl::pair<A,B>` | mcustl_pair.h | Lightweight pair template |
| `mcustl::heap_guard` | mcustl_guard.h | RAII guard for atomic multi-step heap reads |

All components are accessed through a single header:

```cpp
#include "mcustl.h"
```

Do not include sub-headers directly.

---

## Quick Start

```cpp
#include "mcustl.h"

/* Provide a heap buffer (global, linker section, etc.). */
static uint8_t heap_buf[16384];

int main() {
    mcustl_register_heap(heap_buf, sizeof(heap_buf));

    mcustl::vector<int> vec;
    vec.push_back(10);
    vec.push_back(20);

    mcustl::string str("Hello");
    str += " World";

    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_front(2);

    mcustl::map<int, int> m;
    m.insert(1, 100);
    m.insert(2, 200);

    mcustl::smart_ptr<int> ptr;
    ptr.create(42);

    mcustl::json cfg;
    cfg["device"] = mcustl::json("exciter-mini");
    cfg["volume"] = mcustl::json(75);
    auto text = cfg.dump();

    return 0;
}
```

---

## Configuration

mcustl is configured via `mcustl_conf.h`. All parameters have sensible defaults. To override, use one of two approaches:

### Approach 1: Compiler Flags

```bash
gcc -DMAX_NUM_OF_ALLOCATIONS=200 -DALLOCATION_ALIGNMENT_BYTES=8 ...
```

### Approach 2: Custom Configuration File

Create your own config file and pass it via compiler flag:

```bash
gcc -DMCUSTL_CUSTOM_CONF_FILE=\"my_mcustl_conf.h\" ...
```

The custom file is included **before** defaults, so any value you define will not be overridden.

### Example Custom Config (`my_mcustl_conf.h`)

```c
#ifndef MY_MCUSTL_CONF_H
#define MY_MCUSTL_CONF_H

/* Disable modules you don't need to save code size */
#define MCUSTL_USE_LIST         0
#define MCUSTL_USE_MAP          0

/* Trim json: keep dumping/building values, drop the parser and floats */
#define MCUSTL_USE_JSON_PARSER  0
#define MCUSTL_JSON_USE_FLOAT   0

/* Increase allocation limit for complex applications */
#define MAX_NUM_OF_ALLOCATIONS  500UL

/* 8-byte alignment for Cortex-M7 double-word access */
#define ALLOCATION_ALIGNMENT_BYTES  8U

/* Disable memory zeroing for speed (less safe) */
#define FILL_FREED_MEMORY_BY_NULLS  false

/* Redirect debug output to your logger */
#define mcustl_debug  my_log_printf

/* Enable FreeRTOS thread safety */
#define USE_THREAD_SAFETY
#define USE_FREE_RTOS

#endif
```

### All Configuration Parameters

#### Module Switches

| Parameter | Default | Description |
|---|---|---|
| `MCUSTL_USE_VECTOR` | `1` | Enable `mcustl::vector<T>` |
| `MCUSTL_USE_STRING` | `1` | Enable `mcustl::string` (requires VECTOR) |
| `MCUSTL_USE_SMART_PTR` | `1` | Enable `mcustl::smart_ptr<T>`, `observer_ptr<T>` |
| `MCUSTL_USE_LIST` | `1` | Enable `mcustl::list<T>` |
| `MCUSTL_USE_MAP` | `1` | Enable `mcustl::map<K,V>` |
| `MCUSTL_USE_JSON` | `1` | Enable `mcustl::json` (requires VECTOR + STRING + MAP) |
| `MCUSTL_USE_JSON_PARSER` | `1` | Compile in `json::parse` (set 0 to drop parser code) |
| `MCUSTL_USE_JSON_DUMP` | `1` | Compile in `json::dump` (set 0 to drop serializer code) |
| `MCUSTL_JSON_USE_FLOAT` | `1` | Enable `double` numbers (set 0 on MCUs without FPU / soft-float) |
| `MCUSTL_JSON_MAX_DEPTH` | `64` | Parser nesting limit (guards against stack-overflow on hostile input) |

#### Allocator

| Parameter | Default | Description |
|---|---|---|
| `MAX_NUM_OF_ALLOCATIONS` | `100` | Max simultaneous allocations per heap |
| `FILL_FREED_MEMORY_BY_NULLS` | `true` | Zero freed memory (security/debug) |
| `mcustl_debug` | `printf` | Debug output function |
| `mcustl_snprintf` | `snprintf` | snprintf for heap info |

#### Alignment

| Parameter | Default | Description |
|---|---|---|
| `USE_ALIGNMENT` | defined | Enable memory alignment |
| `ALLOCATION_ALIGNMENT_BYTES` | `4` | Alignment boundary in bytes |

#### Single Heap Mode

| Parameter | Default | Description |
|---|---|---|
| `USE_SINGLE_HEAP_MEMORY` | defined | Enable single global heap (default mode) |

Single heap mode simplifies all container constructors: no heap pointer argument needed. To disable (multi-heap mode), `#undef USE_SINGLE_HEAP_MEMORY` in your custom config file.

#### Thread Safety

| Parameter | Default | Description |
|---|---|---|
| `USE_THREAD_SAFETY` | not defined | Enable mutex protection per heap |
| `USE_FREE_RTOS` | not defined | Use FreeRTOS recursive mutex |

When `USE_THREAD_SAFETY` is defined without `USE_FREE_RTOS`, you must provide these macros:

```c
#define MCUSTL_MUTEX_TYPE           your_mutex_type
#define MCUSTL_MUTEX_CREATE()       your_create_mutex()
#define MCUSTL_MUTEX_DELETE(m)      your_delete_mutex(m)
#define MCUSTL_MUTEX_LOCK(m)        your_lock_mutex(m)
#define MCUSTL_MUTEX_UNLOCK(m)      your_unlock_mutex(m)
```

#### Container Tuning

| Parameter | Default | Description |
|---|---|---|
| `MCUSTL_CAPACITY_RESERVE_KOEF` | `1.2` | Vector growth factor (20% extra) |
| `MCUSTL_LIST_CAPACITY_RESERVE_KOEF` | `1.5` | List node pool growth factor |
| `MCUSTL_MAP_CAPACITY_RESERVE_KOEF` | `1.5` | Map node pool growth factor |
| `MCUSTL_MIN_STRING_RESERVE` | `5` | Minimum string capacity |

---

## Containers Reference

### mcustl::pair\<A, B\>

Lightweight pair with `first` and `second` fields. Always available (no allocator dependency).

```cpp
mcustl::pair<int, float> p(1, 3.14f);
p.first;   // 1
p.second;  // 3.14f

auto p2 = mcustl::make_pair(10, 20);

// Comparison operators: ==, !=, <
```

---

### mcustl::vector\<T\>

Dynamic array with contiguous storage. Grows automatically on `push_back`. Supports range-based for loops.

**Complexity:**

| Operation | Time |
|---|---|
| `push_back` | O(1) amortized |
| `pop_back` | O(1) |
| `pop(i)` | O(n) |
| `at(i)` / `operator[]` | O(1) |
| `reserve` | O(n) |
| Range-based for | O(n) |

**API:**

```cpp
mcustl::vector<int> vec;

// Element access
vec.push_back(10);          // -> bool (false on alloc failure)
vec.push_back(20);
vec.push_back(30);
vec.at(0);                  // 10
vec[1];                     // 20
vec.front();                // 10
vec.back();                 // 30
vec.data();                 // T* raw pointer

// Size management
vec.size();                 // 3
vec.capacity();             // >= 3
vec.empty();                // false
vec.reserve(100);           // pre-allocate capacity -> bool
vec.resize(5);              // grow/shrink to size -> bool
vec.resize(10, 0);          // grow and fill with value -> bool
vec.shrink_to_fit();        // release excess memory -> bool
vec.clear();                // remove all elements

// Remove elements
vec.pop_back();             // remove last -> bool
vec.pop(1);                 // remove at index, shift rest -> bool

// Copy and move
mcustl::vector<int> v2 = vec;              // copy
mcustl::vector<int> v3 = static_cast<mcustl::vector<int>&&>(vec);  // move

// Range-based for
for (auto& val : vec) {
    val *= 2;
}

// Const iteration
for (const auto& val : vec) {
    // read-only access
}
```

**Iterator type:** `T*` / `const T*` (raw pointers into contiguous storage).

---

### mcustl::string

Dynamic string backed by `vector<char>`. Null-terminated. Supports concatenation and comparison.

**Complexity:**

| Operation | Time |
|---|---|
| `push_back` | O(1) amortized |
| `append` | O(k), k = appended length |
| `at(i)` / `operator[]` | O(1) |
| `operator==` | O(n) |
| `operator+` | O(n+m) |

**API:**

```cpp
mcustl::string str("Hello");

// Element access
str.at(0);                  // 'H'
str[1];                     // 'e'
str.front();                // 'H'
str.back();                 // 'o'
str.data();                 // char* (null-terminated)
str.c_str();                // const char* (null-terminated)

// Size
str.size();                 // 5
str.length();               // 5 (same as size)
str.empty();                // false

// Modification
str.push_back('!');         // "Hello!" -> bool
str.pop_back();             // "Hello" -> bool
str.append(" World");       // "Hello World" -> bool
str.append(other_str);      // append mcustl::string -> bool
str.append(buf, len);       // append buffer with length -> bool
str += " World";            // same as append -> bool
str += other_str;           // -> bool
str += '!';                 // -> bool

// Assignment
str.assign("New text");     // replace content -> bool
str.assign(buf, len);       // replace from buffer -> bool
str.assign(other_str);      // replace from mcustl::string -> bool

// Capacity
str.reserve(100);           // pre-allocate -> bool
str.resize(10);             // grow/shrink -> bool
str.resize(10, 'X');        // grow and fill -> bool
str.shrink_to_fit();        // release excess -> bool
str.clear();                // empty the string

// Concatenation (creates new string)
mcustl::string result = str + " suffix";
mcustl::string result2 = str + other_str;

// Comparison
str == other_str;           // content comparison
str != other_str;
str < other_str;            // lexicographic

// Copy
mcustl::string s2 = str;
mcustl::string s3;
s3 = str;
```

---

### mcustl::list\<T\>

Doubly-linked list with O(1) insert/erase at any position. Internally uses a pool of nodes stored in a single contiguous allocation. Nodes are linked via indices (not pointers) to survive dalloc defragmentation.

**Complexity:**

| Operation | Time |
|---|---|
| `push_back` / `push_front` | O(1) amortized |
| `pop_back` / `pop_front` | O(1) |
| `insert(iterator, val)` | O(1) amortized |
| `erase(iterator)` | O(1) |
| `remove(val)` | O(n) |
| `front` / `back` | O(1) |
| Range-based for | O(n) |

**API:**

```cpp
mcustl::list<int> lst;

// Add elements
lst.push_back(10);          // -> bool
lst.push_back(20);
lst.push_front(5);          // [5, 10, 20]

// Access ends
lst.front();                // 5
lst.back();                 // 20

// Size
lst.size();                 // 3
lst.empty();                // false
lst.reserve(100);           // pre-allocate node pool -> bool
lst.clear();                // remove all

// Remove from ends
lst.pop_back();             // -> bool
lst.pop_front();            // -> bool

// Remove by value
lst.remove(10);             // remove first matching -> bool
lst.remove_all(10);         // remove all matching -> uint32_t (count)

// Iterator-based insert and erase
auto it = lst.begin();
++it;                       // advance to second element
lst.insert(it, 99);         // insert before it -> iterator
auto next = lst.erase(it);  // erase at it, returns next -> iterator

// Bidirectional iteration
for (auto& val : lst) {
    val *= 2;
}

// Reverse traversal
auto it = lst.end();
while (it != lst.begin()) {
    --it;
    // process *it
}

// Copy and move
mcustl::list<int> l2 = lst;
mcustl::list<int> l3 = static_cast<mcustl::list<int>&&>(lst);
```

**Iterator type:** Bidirectional index-based iterator. Supports `++`, `--`, `*`, `->`, `==`, `!=`.

---

### mcustl::map\<K, V\>

Sorted associative container (key-value pairs). Internally uses a pool-based red-black tree. Keys must support `operator<`. Unique keys only.

**Complexity:**

| Operation | Time |
|---|---|
| `insert` | O(log n) amortized |
| `erase` (by key or iterator) | O(log n) |
| `find` | O(log n) |
| `at` / `operator[]` | O(log n) |
| `lower_bound` / `upper_bound` | O(log n) |
| `contains` / `count` | O(log n) |
| Range-based for (in-order) | O(n) |

**API:**

```cpp
mcustl::map<int, mcustl::string> m;

// Insert
m.insert(3, "three");       // -> bool (false if key exists)
m.insert(1, "one");
m.insert(2, "two");

// Access
m.at(1);                    // "one" (undefined if key missing)
m[1];                       // "one" (inserts default V() if missing)
m.contains(2);              // true
m.count(99);                // 0

// Find returns iterator
auto it = m.find(2);
if (it != m.end()) {
    it->first;              // 2 (key)
    it->second;             // "two" (value)
    it->second = "TWO";     // modify value in-place
}

// Erase
m.erase(3);                 // erase by key -> bool
auto next = m.erase(it);    // erase by iterator -> iterator to next

// Bounds
auto lb = m.lower_bound(2); // first element with key >= 2
auto ub = m.upper_bound(2); // first element with key > 2

// In-order iteration (keys are sorted)
for (auto& kv : m) {
    // kv.first  = key
    // kv.second = value
}

// Size
m.size();                    // number of elements
m.empty();                   // true if empty
m.clear();                   // remove all
m.reserve(50);               // pre-allocate node pool -> bool

// Swap
mcustl::map<int, int> m2;
m.swap(m2);                  // exchange contents

// Copy and move
mcustl::map<int, int> m3 = m;
mcustl::map<int, int> m4 = static_cast<mcustl::map<int, int>&&>(m);
```

**Iterator type:** Bidirectional, dereferences to `mcustl::pair<K, V>`. In-order traversal (ascending key order).

**Value type:** `mcustl::pair<K, V>` with fields `first` (key) and `second` (value).

---

### mcustl::json

Polymorphic JSON value with an `nlohmann::json`-style API. Holds one of: `null`, `bool`, `int64_t`, `double`, `mcustl::string`, `mcustl::vector<json>` (array), `mcustl::map<mcustl::string, json>` (object). Includes a hand-rolled recursive-descent parser and a serializer (both compile-time toggleable).

The implementation depends on `mcustl::vector`, `mcustl::string` and `mcustl::map`, and respects the same heap rules as the rest of the library: every node carries a `heap_t*`, child elements inherit the parent's heap, and copies/moves migrate tracking through `replace_pointers` so defragmentation works transparently.

No `<stdio.h>`, no `malloc`/`new`, no `strtod`/`strtoll` — only `dalloc` + placement-new and a few helpers from `<string.h>` (`memcpy`, `memcmp`, `strcmp`, `strlen`).

**Complexity:**

| Operation | Time |
|---|---|
| Scalar construction / type query / `get_*` | O(1) |
| `operator[](const char*)` (object) | O(n) on first reference, O(n) on miss-then-insert |
| `at(const char*)` / `contains` / `erase(key)` | O(n) |
| `operator[](size_t)` / `at(size_t)` (array) | O(1) |
| `push_back` (array) | O(1) amortized |
| Equality (recursive) | O(n) over both trees |
| `parse` | O(n) over input |
| `dump` | O(n) over the tree, plus output size |

(Object lookup is O(n) by design: it scans the underlying map by `strcmp` against `c_str()` so that callers do not need to materialize a heap-allocated `mcustl::string` key — a stack-allocated key would otherwise leave a dangling tracker entry when its lifetime ends mid-expression.)

**API — construction:**

```cpp
mcustl::json a;                           /* null */
mcustl::json b(nullptr);                  /* null */
mcustl::json c(true);                     /* boolean */
mcustl::json d(42);                       /* number_int */
mcustl::json e(-7LL);
mcustl::json f(3.14);                     /* number_float (if MCUSTL_JSON_USE_FLOAT) */
mcustl::json g("hello");                  /* string from C-string */
mcustl::json h(mcustl::string("world"));  /* string from mcustl::string */

mcustl::json::array_t arr;
arr.push_back(mcustl::json(1));
arr.push_back(mcustl::json("x"));
mcustl::json i(arr);                      /* array */

mcustl::json::object_t obj;
obj.insert(mcustl::string("k"), mcustl::json(1));
mcustl::json j(obj);                      /* object */
```

**API — type queries and accessors:**

```cpp
j.type();                                 /* json::value_t::object | array | string | number_int | number_float | boolean | null */
j.is_null();   j.is_boolean();
j.is_number(); j.is_number_int(); j.is_number_float();
j.is_string(); j.is_array(); j.is_object();

j.size();                                 /* element count for containers, char count for string, 0 otherwise */
j.empty();

j.get_bool();                             /* asserts type is boolean */
j.get_int();                              /* truncates from float if needed */
j.get_float();                            /* coerces from int (only if MCUSTL_JSON_USE_FLOAT) */
j.get_string();                           /* mcustl::string& */
j.get_array();                            /* mcustl::vector<json>& */
j.get_object();                           /* mcustl::map<mcustl::string, json>& */
```

**API — object operations:**

```cpp
mcustl::json cfg;                         /* starts as null */
cfg["volume"] = mcustl::json(80);         /* auto-vivifies cfg into an object */
cfg["mute"]   = mcustl::json(false);
cfg["mode"];                              /* vivified as null */

cfg.contains("volume");                   /* true */
cfg.at("volume").get_int();               /* 80 */
cfg.erase("mute");                        /* true; idempotent (returns false on missing key) */
```

**API — array operations:**

```cpp
mcustl::json log;
log.push_back(mcustl::json("boot"));      /* turns null into an array on first push */
log.push_back(mcustl::json(123));
log.push_back(mcustl::json(3.14));

log.size();                               /* 3 */
log[0].get_string().c_str();              /* "boot" */
log.at((size_t)1).get_int();              /* 123 */
log.erase((size_t)0);                     /* shifts the rest down */
```

**API — iteration:**

```cpp
mcustl::json arr;
for (int i = 0; i < 5; ++i) arr.push_back(mcustl::json(i));
for (auto it = arr.begin(); it != arr.end(); ++it) {
    int v = (int)it->get_int();
    /* it.key() returns an empty string for arrays */
}

mcustl::json obj;
obj["a"] = mcustl::json(1);
obj["b"] = mcustl::json(2);
for (auto it = obj.begin(); it != obj.end(); ++it) {
    const auto& k = it.key();             /* mcustl::string&, sorted (map order) */
    const auto& v = it.value();           /* json& */
}
```

**API — equality:**

```cpp
mcustl::json(42) == mcustl::json(42);     /* true */
mcustl::json(42) == mcustl::json(42.0);   /* true — int↔float numeric compare */
mcustl::json("a") == mcustl::json("a");   /* true */

mcustl::json a; a["k"] = mcustl::json(1);
mcustl::json b; b["k"] = mcustl::json(1);
a == b;                                   /* deep-compare, true */
```

**API — parse (gated by `MCUSTL_USE_JSON_PARSER`):**

```cpp
mcustl::json j = mcustl::json::parse("{\"name\":\"x\",\"vals\":[1,2,3]}");
if (!j.is_null()) {
    j.at("name").get_string();
    j.at("vals")[0].get_int();
}

const char* err = nullptr;
size_t      pos = 0;
mcustl::json bad = mcustl::json::parse("{bad", nullptr, &err, &pos);
if (bad.is_null() && err) {
    /* err is a static C-string description, pos is the byte offset */
}
```

The parser handles all of RFC 8259: integers (up to 18 digits stay in `int64_t`, larger numbers fall back to `double`), fractional / exponential numbers, escapes (`\n \t \r \b \f \" \\ \/`), `\uXXXX` with full UTF-16 surrogate pair → UTF-8 conversion, arbitrary nesting up to `MCUSTL_JSON_MAX_DEPTH`, and arbitrary whitespace.

**API — dump (gated by `MCUSTL_USE_JSON_DUMP`):**

```cpp
mcustl::json j;
j["name"] = mcustl::json("x");
j["vals"] = mcustl::json();
j["vals"].push_back(mcustl::json(1));
j["vals"].push_back(mcustl::json(2));

auto compact = j.dump();                  /* default indent=-1 */
/* {"name":"x","vals":[1,2]} */

auto pretty = j.dump(2);
/* {
 *   "name": "x",
 *   "vals": [
 *     1,
 *     2
 *   ]
 * }
 */
```

Object keys are emitted in sorted order (the underlying `mcustl::map` iterates ascending). Strings are escaped per RFC 8259, including control characters as `\u00XX`. Numbers are formatted by hand-rolled converters with no `printf`/`snprintf` dependency.

**API — multi-heap and clone:**

```cpp
heap_t fast_heap, slow_heap;
heap_init(&fast_heap, fast_buf, sizeof(fast_buf));
heap_init(&slow_heap, slow_buf, sizeof(slow_buf));

mcustl::json fast(&fast_heap);
fast["t"] = mcustl::json(1);              /* child inherits fast_heap */

mcustl::json migrated = fast.clone(&slow_heap);  /* deep copy onto slow_heap */
```

**Notes:**

- `operator[](const char*)` mutates `null` into an `object` on first reference. To create an empty array instead, use `push_back`.
- Returning a reference from `operator[]` is safe for the next single statement, but do not stash it across calls that may allocate (defragmentation can move the underlying map node).
- All allocations are routed through `dalloc`. There is no fallback to system `malloc`.

---

### mcustl::smart_ptr\<T\>

Unique ownership smart pointer. Manages a single dalloc allocation. Non-copyable, move-only. Automatically destructs the managed object and frees memory.

**API:**

```cpp
mcustl::smart_ptr<int> ptr;

// Allocation methods
ptr.create(42);              // allocate + construct with value -> bool
ptr.create();                // allocate + default construct -> bool

// Access
*ptr;                        // 42
ptr.get();                   // T* raw pointer
ptr->member;                 // member access
if (ptr) { /* non-null */ }

// Reset / release
ptr.reset();                 // destruct and free
T* raw = ptr.release();      // release ownership, caller must manage

// Move semantics (no copy)
mcustl::smart_ptr<int> ptr2 = static_cast<mcustl::smart_ptr<int>&&>(ptr);
ptr2 = static_cast<mcustl::smart_ptr<int>&&>(another_ptr);
ptr2 = nullptr;              // same as reset()

// Swap
ptr.swap(ptr2);

// Array allocation
mcustl::smart_ptr<int> arr;
arr.create_array(10);        // allocate 10 ints, default-constructed -> bool
arr.create_array(10, 0);     // allocate 10 ints, initialized to 0 -> bool
arr[0];                      // subscript access

// Factory functions
auto p = mcustl::make_smart<int>(42);
auto a = mcustl::make_smart_array<int>(10);

// Info
ptr.allocated_size();        // number of T elements allocated
ptr.constructed_count();     // number of T elements constructed
ptr.heap();                  // heap_t* pointer
```

### mcustl::smart_ptr\<T[]\>

Array specialization. Raw memory allocation without constructors (for POD buffers).

```cpp
mcustl::smart_ptr<uint8_t[]> buf;
buf.allocate(1024);          // 1024 bytes, no constructors -> bool
buf[0] = 0xFF;               // subscript access
buf.allocated_size();        // 1024
```

### mcustl::observer_ptr\<T\>

Non-owning pointer wrapper. Does not manage lifetime.

```cpp
mcustl::smart_ptr<int> owner;
owner.create(42);

// From smart_ptr
mcustl::observer_ptr<int> obs(owner);
mcustl::observer_ptr<int> obs2 = owner.get_observer();

// From raw pointer
int x = 10;
auto obs3 = mcustl::make_observer(&x);

// Usage
*obs;                        // 42
obs.get();                   // int*
if (obs) { /* non-null */ }
obs.reset();                 // set to null
```

---

### mcustl::heap_guard

RAII wrapper that holds the heap's recursive mutex for the lifetime of its scope. Use it to bracket multi-step reads from tracked containers / json against concurrent allocator activity in other threads.

**When you need it.** mcustl's thread safety guarantees that each *individual* `dalloc`/`dfree` call is atomic and that registered `data_` slots are correctly updated after defragmentation. It does **not** protect a sequence like:

```cpp
const char* p = some_str.c_str();   // reads data_, returns its current value
// — another thread dfree's something here, defrag relocates blocks —
memcpy(dst, p, n);                  // p still holds the OLD address → UB
```

The tracker only updates the `data_` slot of `some_str` after defrag — your local variable `p` is invisible to it. heap_guard plugs that hole by holding the heap mutex across the whole read+use sequence; foreign threads block on their own `dalloc`/`dfree` until you release.

The mutex is recursive (FreeRTOS recursive semaphore / pthread `PTHREAD_MUTEX_RECURSIVE`), so calls to mcustl operations that take the same lock internally (`push_back`, `insert`, `dfree`, etc.) re-enter cleanly.

**API:**

```cpp
{
    mcustl::heap_guard g;        // single-heap mode: locks default heap
    const char* name = j.at("name").get_string().c_str();
    memcpy(buf, name, len);      // foreign defrag cannot run here
}                                 // g destructs → unlock on scope exit

{
    mcustl::heap_guard g(my_heap);   // multi-heap or by-pointer
    /* … */
}
```

Prefer the smallest possible scope. Hold only across the steps that actually need atomicity (typically: walk the json + copy strings to stack buffers); release before doing heavy work like creating modifiers or initializing peripherals.

**Cost.**

- Without `USE_THREAD_SAFETY`: the underlying `heap_lock` / `heap_unlock` collapse to nothing; the guard is a zero-cost RAII wrapper around two no-ops.
- With `USE_THREAD_SAFETY`: one mutex take/give per scope, same as `std::lock_guard<std::recursive_mutex>`.

**Idiomatic example — read config from a shared json safely:**

```cpp
char name[64];
char modifier[64];
{
    mcustl::heap_guard g;
    const auto& entry = cfg.root().at("streams")[i];
    /* copy into stack buffers while no foreign thread can defrag */
    auto n = entry.at("name").get_string();
    memcpy(name, n.c_str(), n.size());
    name[n.size()] = 0;

    auto m = entry.at("modifiers")[0].get_string();
    memcpy(modifier, m.c_str(), m.size());
    modifier[m.size()] = 0;
}   /* unlocked — name / modifier are safe stack copies */

stream->addModifierPreset(modifier);    /* may alloc; we don't care anymore */
```

---

## Memory Architecture

### dalloc Defragmenting Allocator

All mcustl containers use **dalloc** instead of `malloc`/`new`. Key properties:

1. **User-provided heap buffer** -- you control where memory lives (SRAM, DTCM, external RAM, etc.)
2. **Pointer tracking** -- dalloc tracks the **address** of each pointer, not just the value
3. **Defragmentation** -- when fragmentation occurs, dalloc compacts memory and updates all registered tracking pointers automatically
4. **Alignment** -- configurable alignment for all allocations (default 4 bytes)

### Why Indices Instead of Pointers

`list` and `map` use **index-based linking** (not inter-node pointers). This is because dalloc stores each container's data as a single contiguous allocation with one registered tracking pointer (the array base). During defragmentation, dalloc moves the entire block and updates only that base pointer. If nodes pointed to each other via raw pointers, those would become dangling after defrag. Indices relative to the base pointer remain valid.

### Heap Lifecycle

```cpp
// Static buffer (can be placed in linker section)
static uint8_t heap_buf[32768] __attribute__((section(".heap_section")));

// Register once at startup
mcustl_register_heap(heap_buf, sizeof(heap_buf));

// All containers use this heap automatically (single heap mode)
mcustl::vector<int> vec;
mcustl::string str("test");

// Diagnostic output
print_def_dalloc_info();     // print heap status
dump_def_heap();             // hex dump of heap content

// Cleanup (optional, e.g., for tests)
mcustl_reset_heap();         // reset without freeing
mcustl_unregister_heap(true); // full cleanup
```

### Multi-Heap Mode

Disable single heap mode to use multiple independent heaps:

```c
// In custom config:
#undef USE_SINGLE_HEAP_MEMORY
```

```cpp
// Create and initialize heaps manually
static uint8_t fast_buf[8192];
static uint8_t slow_buf[65536];
heap_t fast_heap, slow_heap;

heap_init(&fast_heap, fast_buf, sizeof(fast_buf));
heap_init(&slow_heap, slow_buf, sizeof(slow_buf));

// Pass heap to each container
mcustl::vector<int> fast_vec(&fast_heap);
mcustl::string slow_str(&slow_heap);
mcustl::list<int> fast_list(&fast_heap);
mcustl::map<int, int> slow_map(&slow_heap);
mcustl::smart_ptr<int> ptr(&fast_heap);
```

---

## Thread Safety

Enable thread-safe operation for RTOS environments:

### FreeRTOS

```c
// In custom config:
#define USE_THREAD_SAFETY
#define USE_FREE_RTOS
```

Uses `xSemaphoreCreateRecursiveMutex` / `xSemaphoreTakeRecursive` / `xSemaphoreGiveRecursive`. Each heap gets its own recursive mutex, so independent heaps don't contend.

### Custom RTOS / Bare Metal

```c
#define USE_THREAD_SAFETY

#define MCUSTL_MUTEX_TYPE            osMutexId_t
#define MCUSTL_MUTEX_CREATE()        osMutexNew(NULL)
#define MCUSTL_MUTEX_DELETE(m)       osMutexDelete(m)
#define MCUSTL_MUTEX_LOCK(m)         osMutexAcquire(m, osWaitForever)
#define MCUSTL_MUTEX_UNLOCK(m)       osMutexRelease(m)
```

### POSIX Threads (for host tests)

```c
#define USE_THREAD_SAFETY

#include <pthread.h>

#define MCUSTL_MUTEX_TYPE            pthread_mutex_t*
#define MCUSTL_MUTEX_CREATE()        mcustl_pthread_mutex_create()
#define MCUSTL_MUTEX_DELETE(m)       do { pthread_mutex_destroy(m); free(m); } while(0)
#define MCUSTL_MUTEX_LOCK(m)         pthread_mutex_lock(m)
#define MCUSTL_MUTEX_UNLOCK(m)       pthread_mutex_unlock(m)

static inline pthread_mutex_t* mcustl_pthread_mutex_create() {
    pthread_mutex_t* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
    return m;
}
```

---

## Composability

All containers can be nested arbitrarily:

```cpp
// Vector of strings
mcustl::vector<mcustl::string> names;
names.push_back(mcustl::string("Alice"));
names.push_back(mcustl::string("Bob"));

// Map with string keys and vector values
mcustl::map<mcustl::string, mcustl::vector<int>> data;
mcustl::vector<int> scores;
scores.push_back(95);
scores.push_back(87);
data.insert(mcustl::string("math"), scores);

// Smart pointer to a map
mcustl::smart_ptr<mcustl::map<int, mcustl::string>> ptr;
ptr.create();
ptr->insert(1, mcustl::string("one"));

// List of lists
mcustl::list<mcustl::list<int>> matrix;
mcustl::list<int> row;
row.push_back(1);
row.push_back(2);
matrix.push_back(row);

// json composes vector/string/map under the hood
mcustl::json devices;
for (int i = 0; i < 3; ++i) {
    mcustl::json dev;
    dev["id"]   = mcustl::json(i);
    dev["name"] = mcustl::json("ch");
    devices.push_back(dev);
}
auto wire = devices.dump();
auto back = mcustl::json::parse(wire.c_str());
```

---

## CMake Integration

```cmake
# Add mcustl sources
set(MCUSTL_DIR ${CMAKE_SOURCE_DIR}/libs/mcustl)

# Include paths
target_include_directories(your_target PRIVATE
    ${MCUSTL_DIR}/inc
)

# Source files
target_sources(your_target PRIVATE
    ${MCUSTL_DIR}/src/mcustl_alloc.c
    ${MCUSTL_DIR}/src/mcustl_string.cpp
    ${MCUSTL_DIR}/src/mcustl_json.cpp     # only if MCUSTL_USE_JSON is enabled
)

# Custom configuration (optional)
target_compile_definitions(your_target PRIVATE
    MCUSTL_CUSTOM_CONF_FILE="my_mcustl_conf.h"
)
```

`vector`, `list`, `map`, `smart_ptr`, `pair` are header-only templates. The non-template parts that need a translation unit are:
- `mcustl_alloc.c` -- allocator (C, always required)
- `mcustl_string.cpp` -- string implementation (only if `MCUSTL_USE_STRING` is enabled)
- `mcustl_json.cpp` -- json implementation (only if `MCUSTL_USE_JSON` is enabled)

---

## Error Handling

mcustl uses **return values** instead of exceptions (exceptions are typically disabled on embedded targets):

- All mutating operations that may allocate return `bool`: `true` on success, `false` on allocation failure
- Out-of-range access via `at()`/`operator[]` prints a debug message via `mcustl_debug` and returns a reference to a default-constructed error value
- `smart_ptr` methods return `false` if the pointer is already allocated or the heap is not set

```cpp
mcustl::vector<int> vec;

if (!vec.push_back(42)) {
    // allocation failed -- heap full
}

if (!vec.reserve(1000)) {
    // not enough heap space for 1000 elements
}
```

---

## Sizing the Heap

Rough formula for heap buffer size:

```
heap_size = sum of all container data
          + MAX_NUM_OF_ALLOCATIONS * overhead_per_allocation
          + alignment_padding
```

Each dalloc allocation has a small metadata overhead. With `MAX_NUM_OF_ALLOCATIONS=100` and `ALLOCATION_ALIGNMENT_BYTES=4`, the metadata overhead is approximately 8-16 bytes per allocation depending on the platform.

Practical approach: start with a generous buffer, enable `print_def_dalloc_info()` to observe peak usage, then right-size.

---

## Testing

mcustl includes a comprehensive test suite (338 tests) using Google Test:

```bash
cd mcustl/tests
mkdir build && cd build
cmake ..
make -j$(nproc)
./mcustl_tests
```

Tests cover: basic operations, edge cases, copy/move semantics, defragmentation survival, nested containers, thread safety, stress scenarios, and lifecycle management for non-trivial types.

Tests are built with AddressSanitizer and UndefinedBehaviorSanitizer for memory safety verification.
