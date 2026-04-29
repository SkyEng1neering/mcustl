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
 * @file mcustl_vector.h
 * @brief mcustl::vector<T> - Dynamic array container for embedded systems
 */

#ifndef MCUSTL_VECTOR_H
#define MCUSTL_VECTOR_H

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

#if MCUSTL_USE_VECTOR

#include <new>
#include <type_traits>

namespace mcustl {

template <typename T>
class vector
{
private:
    uint32_t size_val;
    uint32_t capacity_val;
    T *container_ptr;
    T *container_ptr_res;
    heap_t *alloc_mem_ptr;

    T err_retval;

    bool reserve_new_memory(uint32_t new_vect_size, T **cont_ptr);

public:
    vector();

#ifdef USE_SINGLE_HEAP_MEMORY
    vector(uint32_t _size);
#else
    vector(uint32_t _size, heap_t *_alloc_mem_ptr);
    vector(heap_t *_alloc_mem_ptr);
#endif

    vector(const vector &other);
    vector(vector &&other) noexcept;
    ~vector();

    vector<T>& operator = (const vector &vect);
    vector<T>& operator = (vector &&other) noexcept;

    T& at(uint32_t i);
    T& operator[](uint32_t i);
    T& front();
    T& back();
    T* data() const;

    T* begin() { return container_ptr; }
    T* end() { return (T*)((size_t)container_ptr + size_val * sizeof(T)); }
    const T* begin() const { return container_ptr; }
    const T* end() const { return (const T*)((size_t)container_ptr + size_val * sizeof(T)); }

    bool empty() const;
    uint32_t size() const;
    uint32_t capacity() const;
    bool reserve(uint32_t new_capacity);
    bool resize(uint32_t new_vect_size);
    bool resize(uint32_t new_vect_size, T value);
    bool shrink_to_fit();

    void clear();
    bool push_back(T item);
    bool pop_back();
    bool pop(uint32_t i);

    void assign_mem_pointer(heap_t *mem_ptr);
    heap_t* get_mem_pointer() const;

    void info();
};

/* ==================== Implementation ==================== */

template<typename T>
T& vector<T>::at(uint32_t i){
    if((i >= size_val) || (capacity_val == 0)){
        mcustl_debug("mcustl::vector<T>::at(): index %lu is out of range\n", (long unsigned int)i);
        return err_retval;
    }
    else {
        T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
        return *obj_ptr;
    }
}

template<typename T>
T& vector<T>::operator[](uint32_t i){
    return at(i);
}

template<typename T>
T& vector<T>::front(){
    return at(0);
}

template<typename T>
T& vector<T>::back(){
    return at(size_val - 1);
}

template<typename T>
T* vector<T>::data() const{
    return container_ptr;
}

template<typename T>
bool vector<T>::empty() const{
    if(size_val == 0){
        return true;
    }
    else return false;
}

template<typename T>
uint32_t vector<T>::size() const{
    return size_val;
}

template<typename T>
bool vector<T>::reserve_new_memory(uint32_t new_vect_size, T **cont_ptr){
#ifdef USE_SINGLE_HEAP_MEMORY
    if(this->alloc_mem_ptr == NULL){
        this->alloc_mem_ptr = mcustl_get_default_heap();
    }
#endif
    uint32_t alloc_size = sizeof(T)*new_vect_size;
    dalloc(this->alloc_mem_ptr, alloc_size, reinterpret_cast<void**>(cont_ptr));
    if(*cont_ptr != NULL){
        return true;
    }
    else return false;
}

template<typename T>
bool vector<T>::reserve(uint32_t new_capacity){
    if(capacity_val >= new_capacity){
       return true;
    }
    heap_lock(this->alloc_mem_ptr);
    if(reserve_new_memory(new_capacity, &container_ptr_res) != false){
        for(uint32_t i = 0; i < new_capacity; i++){
            new ((T*)((size_t)container_ptr_res + i*sizeof(T))) T;
        }
        for(uint32_t i = 0; i < capacity_val; i++){
            T* obj_new_ptr = (T*)((size_t)container_ptr_res + i*sizeof(T));
            T* obj_old_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
            *obj_new_ptr = static_cast<T&&>(*obj_old_ptr);
        }

        for(uint32_t i = 0; i < capacity_val; i++){
            T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
            obj_ptr->~T();
        }

        if(validate_ptr(this->alloc_mem_ptr, reinterpret_cast<void**>(&container_ptr), USING_PTR_ADDRESS, NULL) != false){
            dfree(this->alloc_mem_ptr, reinterpret_cast<void**>(&container_ptr), USING_PTR_ADDRESS);
        }
        replace_pointers(this->alloc_mem_ptr, reinterpret_cast<void**>(&container_ptr_res), reinterpret_cast<void**>(&container_ptr));
        capacity_val = new_capacity;
        heap_unlock(this->alloc_mem_ptr);
        return true;
    }
    heap_unlock(this->alloc_mem_ptr);
    return false;
}

template<typename T>
uint32_t vector<T>::capacity() const{
    return capacity_val;
}

template<typename T>
void vector<T>::clear(){
    heap_lock(this->alloc_mem_ptr);
    for(uint32_t i = 0; i < size_val; i++){
        {
            T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
            obj_ptr->~T();
        }
        new ((T*)((size_t)container_ptr + i*sizeof(T))) T;
    }
    size_val = 0;
    heap_unlock(this->alloc_mem_ptr);
}

template<typename T>
bool vector<T>::resize(uint32_t new_vect_size){
    if(new_vect_size == size_val){
        return true;
    }
    if(new_vect_size < size_val){
        heap_lock(this->alloc_mem_ptr);
        for(uint32_t i = new_vect_size; i < size_val; i++){
            {
                T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
                obj_ptr->~T();
            }
            new ((T*)((size_t)container_ptr + i*sizeof(T))) T;
        }
        size_val = new_vect_size;
        heap_unlock(this->alloc_mem_ptr);
        return true;
    }

    if(new_vect_size > size_val){
        if(capacity_val >= new_vect_size){
            size_val = new_vect_size;
            return true;
        }

        uint32_t elements_num = static_cast<uint32_t>(((static_cast<float>(new_vect_size)) * MCUSTL_CAPACITY_RESERVE_KOEF));
        if(reserve(elements_num) == true){
            size_val = new_vect_size;
            return true;
        }
    }
    return false;
}

template<typename T>
bool vector<T>::resize(uint32_t new_vect_size, T value){
    uint32_t old_size = size_val;
    heap_lock(this->alloc_mem_ptr);
    if(resize(new_vect_size) != false){
    	if(new_vect_size > old_size){
    		for(uint32_t i = 0; i < new_vect_size - old_size; i++){
				T* obj_ptr = (T*)((size_t)container_ptr + (i + old_size)*sizeof(T));
				*obj_ptr = value;
			}
    	}
        heap_unlock(this->alloc_mem_ptr);
        return true;
    }
    heap_unlock(this->alloc_mem_ptr);
    return false;
}

template<typename T>
bool vector<T>::push_back(T item){
    heap_lock(this->alloc_mem_ptr);
    if(capacity_val > size_val){
        T* obj_ptr = (T*)((size_t)container_ptr + size_val*sizeof(T));
        *obj_ptr = static_cast<T&&>(item);
        size_val++;
        heap_unlock(this->alloc_mem_ptr);
        return true;
    }
    if(resize(size_val + 1) != false){
        T* obj_ptr = (T*)((size_t)container_ptr + (size_val - 1)*sizeof(T));
        *obj_ptr = static_cast<T&&>(item);
        heap_unlock(this->alloc_mem_ptr);
        return true;
    }
    heap_unlock(this->alloc_mem_ptr);
    return false;
}

template<typename T>
bool vector<T>::pop(uint32_t i){
    if(i >= size()){
        return false;
    }

    if(i == size() - 1){
        return pop_back();
    }

    heap_lock(this->alloc_mem_ptr);

    {
        T* removed_obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
        removed_obj_ptr->~T();
    }

    new ((T*)((size_t)container_ptr + i*sizeof(T))) T;

    for(uint32_t k = i; k < size() - 1; k++){
        T* current_obj_ptr = (T*)((size_t)container_ptr + k*sizeof(T));
        T* next_obj_ptr = (T*)((size_t)container_ptr + (k + 1)*sizeof(T));
        *current_obj_ptr = static_cast<T&&>(*next_obj_ptr);
    }

    {
        T* last_obj_ptr = (T*)((size_t)container_ptr + (size_val - 1)*sizeof(T));
        last_obj_ptr->~T();
    }
    new ((T*)((size_t)container_ptr + (size_val - 1)*sizeof(T))) T;

    size_val--;
    heap_unlock(this->alloc_mem_ptr);
    return true;
}

template<typename T>
bool vector<T>::pop_back(){
    if(size_val > 0){
        heap_lock(this->alloc_mem_ptr);
        {
            T* obj_ptr = (T*)((size_t)container_ptr + (size_val - 1)*sizeof(T));
            obj_ptr->~T();
        }
        new ((T*)((size_t)container_ptr + (size_val - 1)*sizeof(T))) T;
        size_val--;
        heap_unlock(this->alloc_mem_ptr);
        return true;
    }
    return false;
}

template<typename T>
bool vector<T>::shrink_to_fit(){
    if((size_val == capacity_val) || (capacity_val == 0)){
       return true;
    }

    heap_lock(this->alloc_mem_ptr);
    if(size_val == 0){
        for(uint32_t i = 0; i < capacity_val; i++){
            T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
            obj_ptr->~T();
        }
        dfree(this->alloc_mem_ptr, reinterpret_cast<void**>(&container_ptr), USING_PTR_ADDRESS);
        capacity_val = 0;
        heap_unlock(this->alloc_mem_ptr);
        return true;
    }

    if(reserve_new_memory(size_val, &container_ptr_res) != false){
        for(uint32_t i = 0; i < size_val; i++){
            new ((T*)((size_t)container_ptr_res + i*sizeof(T))) T;
        }
        for(uint32_t i = 0; i < size_val; i++){
            T* obj_ptr_res = (T*)((size_t)container_ptr_res + i*sizeof(T));
            T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
            *obj_ptr_res = static_cast<T&&>(*obj_ptr);
        }
        for(uint32_t i = 0; i < capacity_val; i++){
            T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
            obj_ptr->~T();
        }
        dfree(this->alloc_mem_ptr, reinterpret_cast<void**>(&container_ptr), USING_PTR_ADDRESS);
        replace_pointers(this->alloc_mem_ptr, reinterpret_cast<void**>(&container_ptr_res), reinterpret_cast<void**>(&container_ptr));
        capacity_val = size_val;
        heap_unlock(this->alloc_mem_ptr);
        return true;
    }
    heap_unlock(this->alloc_mem_ptr);
    return false;
}

template<typename T>
void vector<T>::assign_mem_pointer(heap_t *mem_ptr){
    this->alloc_mem_ptr = mem_ptr;
}

template<typename T>
heap_t* vector<T>::get_mem_pointer() const{
    return this->alloc_mem_ptr;
}

template<typename T>
void vector<T>::info(){
    if(capacity_val == 0){
        mcustl_debug("mcustl::vector info(): empty (capacity=0)\n");
        return;
    }
    mcustl_debug("mcustl::vector info(): start ptr: 0x%08lX, stop ptr: 0x%08lX, total size: %lu bytes\n",
        (unsigned long)(size_t)container_ptr,
        (unsigned long)(size_t)&container_ptr[capacity_val - 1],
        (unsigned long)(capacity_val * sizeof(T)));
}

template<typename T>
vector<T>& vector<T>::operator = (const vector &vect){
    if(&vect != this){
        heap_lock(this->alloc_mem_ptr);
        if(container_ptr != NULL){
            for(uint32_t i = 0; i < capacity_val; i++){
                T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
                obj_ptr->~T();
            }
            dfree(this->alloc_mem_ptr, reinterpret_cast<void**>(&container_ptr), USING_PTR_ADDRESS);
        }
        size_val = 0;
        capacity_val = 0;
        container_ptr = NULL;
        container_ptr_res = NULL;
        this->alloc_mem_ptr = vect.alloc_mem_ptr;
        for(uint32_t i = 0; i < vect.size(); i++){
            this->push_back(vect.data()[i]);
        }
        heap_unlock(this->alloc_mem_ptr);
    }
    return *this;
}

template<typename T>
vector<T>::~vector(){
    if(container_ptr != NULL){
        heap_t* saved_heap = alloc_mem_ptr;
        uint32_t saved_cap = capacity_val;

        heap_lock(saved_heap);

        T* saved_data = NULL;
        replace_pointers(saved_heap, (void**)&container_ptr, (void**)&saved_data);

        /* Guard: a prior defrag may have orphan-removed this vector's
         * tracker (e.g. the containing struct was freed, dropping all
         * inner trackers). In that case replace_pointers can't find
         * &container_ptr, leaves saved_data NULL; the buffer's already
         * gone with the freed parent. Skip — never deref NULL. */
        if (saved_data != NULL) {
            for(uint32_t i = 0; i < saved_cap; i++){
                saved_data[i].~T();
            }

            dfree(saved_heap, (void**)&saved_data, USING_PTR_ADDRESS);
        }

        heap_unlock(saved_heap);
    }
}

template<typename T>
vector<T>::vector(const vector &other){
    size_val = 0;
    capacity_val = 0;
    container_ptr = NULL;
    container_ptr_res = NULL;
    this->alloc_mem_ptr = other.alloc_mem_ptr;
    heap_lock(this->alloc_mem_ptr);
    for(uint32_t i = 0; i < other.size(); i++){
        this->push_back(other.data()[i]);
    }
    heap_unlock(this->alloc_mem_ptr);
}

template<typename T>
vector<T>::vector(vector &&other) noexcept {
    alloc_mem_ptr = other.alloc_mem_ptr;
    size_val = other.size_val;
    capacity_val = other.capacity_val;
    container_ptr = NULL;
    container_ptr_res = NULL;

    if(other.container_ptr != NULL){
        heap_lock(alloc_mem_ptr);
        replace_pointers(alloc_mem_ptr, (void**)&other.container_ptr, (void**)&container_ptr);
        heap_unlock(alloc_mem_ptr);
    }

    other.size_val = 0;
    other.capacity_val = 0;
}

template<typename T>
vector<T>& vector<T>::operator = (vector &&other) noexcept {
    if(this != &other){
        // Free current data
        if(container_ptr != NULL){
            heap_lock(alloc_mem_ptr);
            for(uint32_t i = 0; i < capacity_val; i++){
                T* obj_ptr = (T*)((size_t)container_ptr + i*sizeof(T));
                obj_ptr->~T();
            }
            dfree(alloc_mem_ptr, reinterpret_cast<void**>(&container_ptr), USING_PTR_ADDRESS);
            heap_unlock(alloc_mem_ptr);
        }

        // Transfer from other
        alloc_mem_ptr = other.alloc_mem_ptr;
        size_val = other.size_val;
        capacity_val = other.capacity_val;
        container_ptr = NULL;
        container_ptr_res = NULL;

        if(other.container_ptr != NULL){
            heap_lock(alloc_mem_ptr);
            replace_pointers(alloc_mem_ptr, (void**)&other.container_ptr, (void**)&container_ptr);
            heap_unlock(alloc_mem_ptr);
        }

        other.size_val = 0;
        other.capacity_val = 0;
    }
    return *this;
}

#ifdef USE_SINGLE_HEAP_MEMORY
template<typename T>
vector<T>::vector(){
    alloc_mem_ptr = mcustl_get_default_heap();
    size_val = 0;
    capacity_val = 0;
    container_ptr = NULL;
    container_ptr_res = NULL;
}

template<typename T>
vector<T>::vector(uint32_t _size){
    this->alloc_mem_ptr = mcustl_get_default_heap();
    size_val = 0;
    capacity_val = 0;
    container_ptr = NULL;
    container_ptr_res = NULL;

    reserve(_size);
}
#else
template<typename T>
vector<T>::vector(){
    alloc_mem_ptr = NULL;
    size_val = 0;
    capacity_val = 0;
    container_ptr = NULL;
    container_ptr_res = NULL;
}

template<typename T>
vector<T>::vector(heap_t *_alloc_mem_ptr){
    this->alloc_mem_ptr = _alloc_mem_ptr;
    size_val = 0;
    capacity_val = 0;
    container_ptr = NULL;
    container_ptr_res = NULL;
}

template<typename T>
vector<T>::vector(uint32_t _size, heap_t *_alloc_mem_ptr){
    this->alloc_mem_ptr = _alloc_mem_ptr;
    size_val = 0;
    capacity_val = 0;
    container_ptr = NULL;
    container_ptr_res = NULL;

    reserve(_size);
}
#endif

} // namespace mcustl

#endif // MCUSTL_USE_VECTOR

#endif // MCUSTL_VECTOR_H
