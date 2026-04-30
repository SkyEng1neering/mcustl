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
 * @file mcustl_alloc.h
 * @brief Defragmenting Memory Allocator - Public API
 *
 * Automatic memory defragmentation allocator for embedded systems.
 * Tracks pointer ADDRESSES allowing automatic pointer updates during defrag.
 *
 * @see mcustl_conf.h for configuration options
 * @see mcustl_types.h for type definitions
 */

#ifndef MCUSTL_ALLOC_H
#define MCUSTL_ALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mcustl_types.h"

/* ============================================================================
 * Single Heap API (USE_SINGLE_HEAP_MEMORY)
 * ========================================================================== */

#ifdef USE_SINGLE_HEAP_MEMORY

bool mcustl_register_heap(void *buffer, uint32_t size);
bool mcustl_unregister_heap(bool force);
void mcustl_reset_heap(void);
bool mcustl_is_initialized(void);
heap_t* mcustl_get_default_heap(void);

void def_dalloc(uint32_t size, void **ptr);
void def_dfree(void **ptr);
bool def_drealloc(uint32_t size, void **ptr);
void def_replace_pointers(void **ptr_to_replace, void **ptr_new);
void def_heap_lock(void);
void def_heap_unlock(void);

void print_def_dalloc_info(void);
int def_dalloc_heap_info_str(char *buf, uint32_t buf_size);
void dump_def_heap(void);
void dump_def_dalloc_ptr_info(void);

#endif /* USE_SINGLE_HEAP_MEMORY */

/* ============================================================================
 * Multi-Heap API (Explicit heap parameter)
 * ========================================================================== */

void heap_init(heap_t *heap_struct_ptr, void *mem_ptr, uint32_t mem_size);
void heap_deinit(heap_t *heap_struct_ptr);

void dalloc(heap_t *heap_struct_ptr, uint32_t size, void **ptr);
void dfree(heap_t *heap_struct_ptr, void **ptr, validate_ptr_condition_t condition);
bool drealloc(heap_t *heap_struct_ptr, uint32_t size, void **ptr);
bool validate_ptr(heap_t *heap_struct_ptr, void **ptr,
                  validate_ptr_condition_t condition, uint32_t *ptr_index);
void replace_pointers(heap_t *heap_struct_ptr, void **ptr_to_replace, void **ptr_new);

void heap_lock(heap_t *heap_struct_ptr);
void heap_unlock(heap_t *heap_struct_ptr);

/* ============================================================================
 * Pseudo-trackers (self-pointer tracking for heap-allocated `this`)
 * ----------------------------------------------------------------------------
 * Methods on heap-allocated containers face the "stale this" problem: once
 * the method calls dfree, defrag may relocate the block holding *this. The
 * compiler's `this` register copy still points at the OLD address. Reading
 * or writing through `this->member` after that point lands in the wrong
 * struct.
 *
 * A pseudo-tracker is a stack-local pointer registered in the heap's tracker
 * array WITHOUT any backing allocation (allocated_size==0). Defrag's
 * value-shift pass updates the stored pointer value the same way it updates
 * any other tracked pointer — so a `T* self` registered at method entry
 * tracks the moving `*this` automatically. Use `self->...` inside the method
 * instead of `this->...` and the code stays correct across defrags.
 *
 * Pseudo-trackers must be unregistered before the stack frame goes away.
 * Prefer the C++ `mcustl::tracked_this<T>` RAII helper from mcustl_guard.h.
 * ========================================================================== */
void register_pseudo_tracker(heap_t *heap_struct_ptr, void **self_addr);
void unregister_pseudo_tracker(heap_t *heap_struct_ptr, void **self_addr);

void print_dalloc_info(heap_t *heap_struct_ptr);
int dalloc_heap_info_str(heap_t *heap_struct_ptr, char *buf, uint32_t buf_size);
void dump_heap(heap_t *heap_struct_ptr);
void dump_dalloc_ptr_info(heap_t *heap_struct_ptr);

#ifdef __cplusplus
}
#endif

#endif /* MCUSTL_ALLOC_H */
