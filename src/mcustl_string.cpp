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

#include "mcustl.h"

#if MCUSTL_USE_STRING

#include <cstring>

namespace mcustl {

/* tracked_heap_ptr lives in mcustl_guard.h — used here and by external
 * project-side wrappers that snapshot a heap pointer across a dfree. */

char& string::at(uint32_t i){
	return ch_container.at(i);
}

char& string::operator[](uint32_t i){
	return at(i);
}

char& string::front(){
	return ch_container.front();
}

char& string::back(){
	if(ch_container.size() > 1){
		return ch_container.at(ch_container.size() - 2);
	}
	return ch_container.back();
}

char* string::data() const{
	return ch_container.data();
}

const char* string::c_str() const{
	return data();
}

bool string::empty(){
	return size() == 0;
}

uint32_t string::size() const{
	if(ch_container.size() > 0){
		return ch_container.size() - 1;
	}
	return ch_container.size();
}

uint32_t string::length() const{
	return size();
}

bool string::reserve(uint32_t new_string_size){
	return ch_container.reserve(new_string_size + 1);
}

uint32_t string::capacity(){
	return ch_container.capacity();
}

bool string::shrink_to_fit(){
	return ch_container.shrink_to_fit();
}

void string::clear(){
	ch_container.clear();
}

bool string::push_back(char item){
	/* ch_container.push_back may dfree+defrag and relocate `*this`. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
	if(self->ch_container.size() > 0){
		if(self->ch_container.pop_back() != true){
			return false;
		}
	}
	if(self->ch_container.push_back(item) != true){
		return false;
	}
	return self->ch_container.push_back(0);
}

bool string::pop_back(){
	if(size() == 0){
		return false;
	}

	/* ch_container.push_back at the end may dfree+defrag and relocate `*this`. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());

	if(self->ch_container.pop_back() != true){
		return false;
	}

	if(self->ch_container.pop_back() != true){
		return false;
	}

	return self->ch_container.push_back(0);
}

bool string::append(const char *str){
	/* push_back loop may dfree+defrag and relocate `*this`. The same
	 * defrag may relocate the buffer `str` aliases (when `str` came
	 * from another mcustl string in heap). Pseudo-track our local copy
	 * of `str` so it follows that buffer through any number of moves. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
	if(str[0] == '\0'){
		return false;
	}
	const char* p = str;
	tracked_heap_ptr _tp(self->ch_container.get_mem_pointer(), &p);
	while(*p != '\0'){
		char ch = *p;
		if(self->push_back(ch) != true){
			return false;
		}
		++p;
	}
	return true;
}

bool string::append(char *str, uint32_t str_len){
	/* See append(const char*) — same heap-relocation hazard for `str`. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
	const char* p = str;
	tracked_heap_ptr _tp(self->ch_container.get_mem_pointer(), &p);
	for(uint32_t i = 0; i < str_len; i++){
		char ch = p[i];
		if(self->push_back(ch) != true){
			return false;
		}
	}
	return true;
}

bool string::append(const char *str, uint32_t str_len){
	return append(const_cast<char*>(str), str_len);
}

bool string::append(const string& str){
	/* reserve + push_back loop both may dfree+defrag and relocate
	 * `*this`. The same defrag may also relocate `str` itself when it's
	 * a heap-resident object (e.g. a key stored inside a map node), so
	 * we cannot read through `str` after the first allocation here.
	 * Snapshot src_data + size at function entry while `str` is still
	 * valid, then pseudo-track src_data through the rest. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
	uint32_t str_len = str.size();
	const char* src_data = str.c_str();

	heap_t* h = self->ch_container.get_mem_pointer();
	tracked_heap_ptr _tp(h, &src_data);

	if(self->reserve(self->size() + str_len) != true){
		return false;
	}
	for (uint32_t i = 0; i < str_len; i++) {
		if (!self->push_back(src_data[i])) {
			return false;
		}
	}
	return true;
}

bool string::append(char ch){
	return push_back(ch);
}

bool string::operator+=(const char *str){
	return append(str);
}

bool string::operator+=(const string& str){
	return append(str);
}

bool string::operator+=(char ch){
	return append(ch);
}

bool string::operator==(const string &str) const {
	if (size() != str.size()) {
		return false;
	}
	if (strncmp(ch_container.data(), str.data(), str.size()) != 0) {
		return false;
	}
	return true;
}

bool string::operator==(string &str) const {
	if (size() != str.size()) {
		return false;
	}
	if (strncmp(ch_container.data(), str.data(), str.size()) != 0) {
		return false;
	}
	return true;
}

bool string::operator!=(const string &name) const {
	return !(*this == name);
}

bool string::operator!=(string &name) const {
	return !(*this == name);
}

bool string::operator<(const string &other) const {
	uint32_t min_len = size() < other.size() ? size() : other.size();
	/* strncmp(NULL, NULL, 0) is UB per the C standard even though the
	 * result is trivially 0 — guard so empty-string comparisons (real
	 * for fresh map keys / cleared strings) don't trip UBSan. */
	if (min_len > 0) {
		int cmp = strncmp(ch_container.data(), other.data(), min_len);
		if (cmp != 0) return cmp < 0;
	}
	return size() < other.size();
}

bool string::resize(uint32_t new_str_size){
	return resize(new_str_size, 0);
}

bool string::resize(uint32_t new_str_size, char value){
	if(size() == new_str_size){
		return true;
	}

	/* pop_back / reserve / ch_container.resize may dfree+defrag and relocate
	 * `*this`. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());

	if(self->size() > new_str_size){
		uint32_t to_remove = self->size() - new_str_size;
		for(uint32_t i = 0; i < to_remove; i++){
			self->pop_back();
		}
		return true;
	}

	if(self->capacity() <= new_str_size){
		if(self->reserve(new_str_size) != true){
			return false;
		}
	}

	if(self->ch_container.resize(new_str_size + 1, value) != true){
		return false;
	}
	self->ch_container.at(new_str_size) = '\0';

	return true;
}

bool string::assign(const char *str){
	/* push_back loop may dfree+defrag and relocate `*this`. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
	uint32_t ind = 0;
	if(str[ind] == '\0'){
		return false;
	}
	self->ch_container.clear();
	while(str[ind] != '\0'){
		if(self->push_back(str[ind]) != true){
			return false;
		}
		ind++;
	}
	return true;
}

bool string::assign(char *str, uint32_t str_len){
	/* push_back loop may dfree+defrag and relocate `*this`. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
	self->ch_container.clear();
	for(uint32_t i = 0; i < str_len; i++){
		if(self->push_back(str[i]) != true){
			return false;
		}
	}
	return true;
}

bool string::assign(const char *str, uint32_t str_len){
	return assign(const_cast<char*>(str), str_len);
}

bool string::assign(const string& str){
	if(this == &str){
		return true;
	}
	/* clear + assign loop go through vector::push_back which may dfree+
	 * defrag and relocate `*this`. */
	MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
	uint32_t str_len = str.size();
	self->ch_container.clear();
	return self->assign(str.c_str(), str_len);
}

heap_t* string::get_mem_pointer() const{
    return ch_container.get_mem_pointer();
}

#ifdef USE_SINGLE_HEAP_MEMORY
string::string(uint32_t _size){
    resize(_size);
}

string::string(const char *str){
    assign(str);
}

string::string(const string &other){
    /* push_back loop may dfree+defrag and relocate `*this` and ALSO
     * relocate `other` when it lives in heap. Snapshot src state and
     * pseudo-track the source buffer; then never read through `other`. */
    MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
    uint32_t      src_size = other.size();
    const char*   src_data = other.data();

    heap_t* h = this->ch_container.get_mem_pointer();
    tracked_heap_ptr _tp(h, &src_data);

    for(uint32_t i = 0; i < src_size; i++){
        self->push_back(src_data[i]);
    }
}

string& string::operator = (const string &other){
    if(&other != this){
        /* See copy ctor — same heap-relocation hazard for `other`. */
        MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
        uint32_t      src_size = other.size();
        const char*   src_data = other.data();

        heap_t* h = this->ch_container.get_mem_pointer();
        tracked_heap_ptr _tp(h, &src_data);

        self->clear();
        for(uint32_t i = 0; i < src_size; i++){
            self->push_back(src_data[i]);
        }
    }
    return *this;
}

string string::operator + (string &str){
    uint32_t self_str_len = this->size();
    uint32_t new_str_len = str.size();
    string new_string(self_str_len + new_str_len);
    new_string.clear();

    for(uint32_t i = 0; i < self_str_len; i++){
        new_string.push_back(this->at(i));
    }
    for(uint32_t i = 0; i < new_str_len; i++){
        new_string.push_back(str.at(i));
    }
    return new_string;
}

string string::operator + (const char *str){
    uint32_t self_str_len = this->size();
    uint32_t new_str_len = strlen(str);

    string new_string(self_str_len + new_str_len);
    new_string.clear();

    for(uint32_t i = 0; i < self_str_len; i++){
        new_string.push_back(this->at(i));
    }
    for(uint32_t i = 0; i < new_str_len; i++){
        new_string.push_back(str[i]);
    }
    return new_string;
}
#else
void string::assign_mem_pointer(heap_t *mem_ptr){
    ch_container.assign_mem_pointer(mem_ptr);
}

string::string(uint32_t _size, heap_t *_alloc_mem_ptr){
    ch_container.assign_mem_pointer(_alloc_mem_ptr);
    resize(_size);
}

string::string(heap_t *_alloc_mem_ptr){
    ch_container.assign_mem_pointer(_alloc_mem_ptr);
}

string::string(const char *str, heap_t *_alloc_mem_ptr){
    ch_container.assign_mem_pointer(_alloc_mem_ptr);
    assign(str);
}

string::string(const string &other){
    this->assign_mem_pointer(other.get_mem_pointer());
    /* push_back loop may dfree+defrag and relocate `*this` and ALSO
     * relocate `other` when other lives in heap (e.g. a key stored inside
     * a map node). Snapshot other's state up-front and pseudo-track the
     * source buffer pointer on its OWNING heap; then we never read
     * through `other` again. */
    MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
    uint32_t      src_size = other.size();
    const char*   src_data = other.data();
    heap_t*       src_heap = other.get_mem_pointer();
    tracked_heap_ptr _tp(src_heap, &src_data);

    for(uint32_t i = 0; i < src_size; i++){
        self->push_back(src_data[i]);
    }
}

string& string::operator = (const string &other){
    if(&other != this){
        this->assign_mem_pointer(other.get_mem_pointer());
        /* See copy ctor — same heap-relocation hazard for `other`. */
        MCUSTL_TRACKED_THIS(string, this->ch_container.get_mem_pointer());
        uint32_t      src_size = other.size();
        const char*   src_data = other.data();
        heap_t*       src_heap = other.get_mem_pointer();
        tracked_heap_ptr _tp(src_heap, &src_data);

        self->clear();
        for(uint32_t i = 0; i < src_size; i++){
            self->push_back(src_data[i]);
        }
    }
    return *this;
}

string string::operator + (string &str){
    uint32_t self_str_len = this->size();
    uint32_t new_str_len = str.size();
    string new_string(self_str_len + new_str_len, this->ch_container.get_mem_pointer());
    new_string.clear();

    for(uint32_t i = 0; i < self_str_len; i++){
        new_string.push_back(this->at(i));
    }
    for(uint32_t i = 0; i < new_str_len; i++){
        new_string.push_back(str.at(i));
    }
    return new_string;
}

string string::operator + (const char *str){
    uint32_t self_str_len = this->size();
    uint32_t new_str_len = strlen(str);

    string new_string(self_str_len + new_str_len, this->ch_container.get_mem_pointer());
    new_string.clear();

    for(uint32_t i = 0; i < self_str_len; i++){
        new_string.push_back(this->at(i));
    }
    for(uint32_t i = 0; i < new_str_len; i++){
        new_string.push_back(str[i]);
    }
    return new_string;
}
#endif

string::string(){
}

string::~string(){
}

} // namespace mcustl

#endif // MCUSTL_USE_STRING
