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
 * @file mcustl_conf.h
 * @brief Unified configuration for mcustl library
 *
 * This file contains all configurable parameters for mcustl. You can override
 * any setting in two ways:
 *
 * 1. Compiler flags:
 *    @code
 *    gcc -DMAX_NUM_OF_ALLOCATIONS=200 ...
 *    @endcode
 *
 * 2. Custom configuration file:
 *    @code
 *    gcc -DMCUSTL_CUSTOM_CONF_FILE=\"my_config.h\" ...
 *    @endcode
 *
 * Module configuration:
 * - MCUSTL_USE_VECTOR: Enable mcustl::vector (default: enabled)
 * - MCUSTL_USE_STRING: Enable mcustl::string (default: enabled, requires MCUSTL_USE_VECTOR)
 * - MCUSTL_USE_SMART_PTR: Enable mcustl::smart_ptr / mcustl::observer_ptr (default: enabled)
 * - MCUSTL_USE_LIST: Enable mcustl::list (default: enabled)
 * - MCUSTL_USE_MAP: Enable mcustl::map (default: enabled)
 * - MCUSTL_USE_JSON: Enable mcustl::json (default: enabled, requires MAP+VECTOR+STRING)
 *
 * Allocator configuration:
 * - USE_SINGLE_HEAP_MEMORY (default: enabled)
 * - MAX_NUM_OF_ALLOCATIONS, FILL_FREED_MEMORY_BY_NULLS
 * - USE_ALIGNMENT, ALLOCATION_ALIGNMENT_BYTES
 * - USE_THREAD_SAFETY, USE_FREE_RTOS
 */

#ifndef MCUSTL_CONF_H
#define MCUSTL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Custom configuration support:
 * Define MCUSTL_CUSTOM_CONF_FILE to include your own configuration file.
 * Example: -DMCUSTL_CUSTOM_CONF_FILE="my_mcustl_conf.h"
 *
 * In your custom config, define only the values you want to override.
 * Any undefined values will use the defaults below.
 */
#ifdef MCUSTL_CUSTOM_CONF_FILE
    #include MCUSTL_CUSTOM_CONF_FILE
#endif

/* ==================== Library Version ==================== */

#ifndef MCUSTL_VERSION
#define MCUSTL_VERSION                      "1.2.1"
#endif

/* ==================== Module Enable/Disable ==================== */

/* Enable mcustl::vector<T> container */
#ifndef MCUSTL_USE_VECTOR
#define MCUSTL_USE_VECTOR                   1
#endif

/* Enable mcustl::string container (requires MCUSTL_USE_VECTOR) */
#ifndef MCUSTL_USE_STRING
#define MCUSTL_USE_STRING                   1
#endif

/* Enable mcustl::smart_ptr<T> and mcustl::observer_ptr<T> */
#ifndef MCUSTL_USE_SMART_PTR
#define MCUSTL_USE_SMART_PTR                1
#endif

/* Enable mcustl::list<T> container */
#ifndef MCUSTL_USE_LIST
#define MCUSTL_USE_LIST                     1
#endif

/* Enable mcustl::map<K,V> container */
#ifndef MCUSTL_USE_MAP
#define MCUSTL_USE_MAP                      1
#endif

/* Enable mcustl::json (depends on map + vector + string) */
#ifndef MCUSTL_USE_JSON
#define MCUSTL_USE_JSON                     1
#endif

/* ==================== Dependency Checks ==================== */

#if MCUSTL_USE_STRING && !MCUSTL_USE_VECTOR
#error "MCUSTL_USE_STRING requires MCUSTL_USE_VECTOR (string uses vector<char> internally)"
#endif

#if MCUSTL_USE_JSON && (!MCUSTL_USE_MAP || !MCUSTL_USE_VECTOR || !MCUSTL_USE_STRING)
#error "MCUSTL_USE_JSON requires MCUSTL_USE_MAP, MCUSTL_USE_VECTOR and MCUSTL_USE_STRING"
#endif

/* ==================== Allocator Configuration ==================== */

/* Fill freed memory with zeros for security/debugging */
#ifndef FILL_FREED_MEMORY_BY_NULLS
#define FILL_FREED_MEMORY_BY_NULLS          true
#endif

/* Debug output function (set to empty macro to disable) */
#ifndef mcustl_debug
#define mcustl_debug                        printf
#endif

/* snprintf-like function for heap info strings */
#ifndef mcustl_snprintf
#define mcustl_snprintf                     snprintf
#endif

/* Maximum number of simultaneous allocations */
#ifndef MAX_NUM_OF_ALLOCATIONS
#define MAX_NUM_OF_ALLOCATIONS              100UL
#endif

/* ==================== Alignment Configuration ==================== */

/* Define USE_ALIGNMENT to enable memory alignment (recommended) */
#ifndef USE_ALIGNMENT
#define USE_ALIGNMENT
#endif

#ifdef USE_ALIGNMENT
    #ifndef ALLOCATION_ALIGNMENT_BYTES
    #define ALLOCATION_ALIGNMENT_BYTES      4U
    #endif

    /* Align value up to the nearest multiple of ALLOCATION_ALIGNMENT_BYTES */
    #ifndef MCUSTL_ALIGN_UP
    #define MCUSTL_ALIGN_UP(val) (((val) + (ALLOCATION_ALIGNMENT_BYTES - 1U)) & ~(ALLOCATION_ALIGNMENT_BYTES - 1U))
    #endif
#endif

/* ==================== Single Heap Configuration ==================== */

/*
 * USE_SINGLE_HEAP_MEMORY enables a single global heap (default mode).
 * Provides simplified constructors for all containers (no heap pointer needed).
 *
 * Usage:
 *   1. Create a buffer (can be placed in specific memory section)
 *   2. Call mcustl_register_heap(buffer, size) before any allocations
 *
 * To disable (multi-heap mode), #undef USE_SINGLE_HEAP_MEMORY
 * in your MCUSTL_CUSTOM_CONF_FILE before this header's defaults apply.
 */
#ifndef USE_SINGLE_HEAP_MEMORY
#define USE_SINGLE_HEAP_MEMORY
#endif

/* ==================== Thread Safety Configuration ==================== */

/*
 * Define USE_THREAD_SAFETY to enable thread-safe operations.
 * Each heap will have its own mutex for protection.
 *
 * If USE_FREE_RTOS is also defined, FreeRTOS mutex primitives will be used.
 * Otherwise, define the following macros:
 *
 *   MCUSTL_MUTEX_TYPE              - The mutex handle type
 *   MCUSTL_MUTEX_CREATE()          - Create and return a mutex handle
 *   MCUSTL_MUTEX_DELETE(mutex)     - Delete/destroy the mutex
 *   MCUSTL_MUTEX_LOCK(mutex)       - Acquire the mutex (blocking)
 *   MCUSTL_MUTEX_UNLOCK(mutex)     - Release the mutex
 *
 * Not defined by default. Enable via compiler flag (-DUSE_THREAD_SAFETY)
 * or in your MCUSTL_CUSTOM_CONF_FILE.
 */

#ifdef USE_THREAD_SAFETY

/* FreeRTOS implementation */
#ifdef USE_FREE_RTOS
    #include "FreeRTOS.h"
    #include "semphr.h"

    #ifndef MCUSTL_MUTEX_TYPE
    #define MCUSTL_MUTEX_TYPE               SemaphoreHandle_t
    #endif

    #ifndef MCUSTL_MUTEX_CREATE
    #define MCUSTL_MUTEX_CREATE()           xSemaphoreCreateRecursiveMutex()
    #endif

    #ifndef MCUSTL_MUTEX_DELETE
    #define MCUSTL_MUTEX_DELETE(mutex)      do { if(mutex) vSemaphoreDelete(mutex); } while(0)
    #endif

    #ifndef MCUSTL_MUTEX_LOCK
    #define MCUSTL_MUTEX_LOCK(mutex)        do { if(mutex) xSemaphoreTakeRecursive(mutex, portMAX_DELAY); } while(0)
    #endif

    #ifndef MCUSTL_MUTEX_UNLOCK
    #define MCUSTL_MUTEX_UNLOCK(mutex)      do { if(mutex) xSemaphoreGiveRecursive(mutex); } while(0)
    #endif

#else /* Custom OS - user must define mutex macros */

    #ifndef MCUSTL_MUTEX_TYPE
    #error "USE_THREAD_SAFETY defined but MCUSTL_MUTEX_TYPE is not defined. Define mutex macros or enable USE_FREE_RTOS."
    #endif

    #ifndef MCUSTL_MUTEX_CREATE
    #error "USE_THREAD_SAFETY defined but MCUSTL_MUTEX_CREATE is not defined."
    #endif

    #ifndef MCUSTL_MUTEX_DELETE
    #error "USE_THREAD_SAFETY defined but MCUSTL_MUTEX_DELETE is not defined."
    #endif

    #ifndef MCUSTL_MUTEX_LOCK
    #error "USE_THREAD_SAFETY defined but MCUSTL_MUTEX_LOCK is not defined."
    #endif

    #ifndef MCUSTL_MUTEX_UNLOCK
    #error "USE_THREAD_SAFETY defined but MCUSTL_MUTEX_UNLOCK is not defined."
    #endif

#endif /* USE_FREE_RTOS */

#endif /* USE_THREAD_SAFETY */

/* ==================== Smart Pointer Configuration ==================== */

#if MCUSTL_USE_SMART_PTR

/* Debug output for smart pointer (disabled by default) */
#ifndef MCUSTL_SMART_PTR_DEBUG
#define MCUSTL_SMART_PTR_DEBUG(...)  ((void)0)
#endif

/* Debug assertions - define MCUSTL_SMART_PTR_ENABLE_ASSERTIONS to enable */
#ifdef MCUSTL_SMART_PTR_ENABLE_ASSERTIONS
    #ifndef MCUSTL_SMART_PTR_ASSERT
        #include <cassert>
        #define MCUSTL_SMART_PTR_ASSERT(cond, msg) assert((cond) && (msg))
    #endif
#else
    #ifndef MCUSTL_SMART_PTR_ASSERT
    #define MCUSTL_SMART_PTR_ASSERT(cond, msg) ((void)0)
    #endif
#endif

#endif /* MCUSTL_USE_SMART_PTR */

/* ==================== Vector Configuration ==================== */

#if MCUSTL_USE_VECTOR

/* Capacity growth factor when resizing (20% extra space) */
#ifndef MCUSTL_CAPACITY_RESERVE_KOEF
#define MCUSTL_CAPACITY_RESERVE_KOEF        1.2
#endif

#endif /* MCUSTL_USE_VECTOR */

/* ==================== String Configuration ==================== */

#if MCUSTL_USE_STRING

/* Minimum string capacity reservation */
#ifndef MCUSTL_MIN_STRING_RESERVE
#define MCUSTL_MIN_STRING_RESERVE           5
#endif

#endif /* MCUSTL_USE_STRING */

/* ==================== List Configuration ==================== */

#if MCUSTL_USE_LIST

/* Capacity growth factor when resizing list node pool */
#ifndef MCUSTL_LIST_CAPACITY_RESERVE_KOEF
#define MCUSTL_LIST_CAPACITY_RESERVE_KOEF   1.5
#endif

#endif /* MCUSTL_USE_LIST */

/* ==================== Map Configuration ==================== */

#if MCUSTL_USE_MAP

/* Capacity growth factor when resizing map node pool */
#ifndef MCUSTL_MAP_CAPACITY_RESERVE_KOEF
#define MCUSTL_MAP_CAPACITY_RESERVE_KOEF    1.5
#endif

#endif /* MCUSTL_USE_MAP */

/* ==================== JSON Configuration ==================== */

#if MCUSTL_USE_JSON

/* Enable JSON text parsing (json::parse). Disable to drop the parser code
 * if you only need to construct/serialize JSON values programmatically. */
#ifndef MCUSTL_USE_JSON_PARSER
#define MCUSTL_USE_JSON_PARSER              1
#endif

/* Enable JSON serialization (json::dump). Disable to drop the dump code. */
#ifndef MCUSTL_USE_JSON_DUMP
#define MCUSTL_USE_JSON_DUMP                1
#endif

/* Enable double (number_float) support. When 0, only int64 numbers are
 * supported — useful on MCUs without an FPU or to avoid pulling in soft
 * float helpers. Float construction, get_float, parse of fractional/exp
 * numbers, and dump of floats become unavailable. */
#ifndef MCUSTL_JSON_USE_FLOAT
#define MCUSTL_JSON_USE_FLOAT               1
#endif

/* Maximum nesting depth the parser will accept. Guards against stack
 * overflow on attacker-controlled input. */
#ifndef MCUSTL_JSON_MAX_DEPTH
#define MCUSTL_JSON_MAX_DEPTH               64
#endif

#endif /* MCUSTL_USE_JSON */

#ifdef __cplusplus
}
#endif

#endif /* MCUSTL_CONF_H */
