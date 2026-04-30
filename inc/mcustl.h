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
 * @file mcustl.h
 * @brief MCU Standard Template Library - Unified header
 *
 * Include this single header to access all mcustl components:
 *
 *   mcustl::pair<A,B>       - Lightweight pair (always available)
 *   mcustl::vector<T>       - Dynamic array
 *   mcustl::string           - Dynamic string (backed by vector<char>)
 *   mcustl::list<T>          - Doubly-linked list (pool-based, index-linked)
 *   mcustl::map<K,V>         - Sorted associative container (pool-based red-black tree)
 *   mcustl::smart_ptr<T>    - Unique ownership smart pointer
 *   mcustl::observer_ptr<T> - Non-owning pointer
 *
 * All containers use the dalloc defragmenting memory allocator.
 * Single-heap mode is enabled by default (USE_SINGLE_HEAP_MEMORY).
 *
 * Configuration: see mcustl_conf.h or define MCUSTL_CUSTOM_CONF_FILE
 *
 * Usage:
 * @code
 *   #include "mcustl.h"
 *
 *   uint8_t heap_buf[8192];
 *   mcustl_register_heap(heap_buf, sizeof(heap_buf));
 *
 *   mcustl::vector<int> vec;
 *   vec.push_back(42);
 *
 *   mcustl::string str("Hello");
 *   str += " World";
 *
 *   mcustl::list<int> lst;
 *   lst.push_back(1);
 *
 *   mcustl::map<int, int> m;
 *   m.insert(1, 100);
 *
 *   mcustl::smart_ptr<int> ptr;
 *   ptr.create(100);
 * @endcode
 */

#ifndef MCUSTL_H
#define MCUSTL_H

/* Always include allocator (foundation for everything) */
#include "mcustl_alloc.h"

/* RAII heap mutex guard + tracked_this helper — needed by every container
 * implementation, so it must be visible before they're included. */
#include "mcustl_guard.h"

/* Pair is always available (no allocator dependency) */
#include "mcustl_pair.h"

/* Conditionally include modules based on configuration */
#if MCUSTL_USE_VECTOR
#include "mcustl_vector.h"
#endif

#if MCUSTL_USE_STRING
#include "mcustl_string.h"
#endif

#if MCUSTL_USE_SMART_PTR
#include "mcustl_smart_ptr.h"
#endif

#if MCUSTL_USE_LIST
#include "mcustl_list.h"
#endif

#if MCUSTL_USE_MAP
#include "mcustl_map.h"
#endif

#if MCUSTL_USE_JSON
#include "mcustl_json.h"
#endif

#endif /* MCUSTL_H */
