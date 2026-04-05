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
 * @file mcustl_string.h
 * @brief mcustl::string - Dynamic string class for embedded systems
 */

#ifndef MCUSTL_STRING_H
#define MCUSTL_STRING_H

#ifndef MCUSTL_H
#error "Do not include this header directly. Include mcustl.h instead."
#endif

#if MCUSTL_USE_STRING

namespace mcustl {

class string
{
private:
    vector<char> ch_container;

public:
    string();

#ifdef USE_SINGLE_HEAP_MEMORY
    string(uint32_t _size);
    string(const char *str);
#else
    string(heap_t *_alloc_mem_ptr);
    string(uint32_t _size, heap_t *_alloc_mem_ptr);
    string(const char *str, heap_t *_alloc_mem_ptr);
    void assign_mem_pointer(heap_t *mem_ptr);
#endif

    string(const string &other);
    ~string();

    string& operator = (const string &other);

    char& at(uint32_t i);
    char& operator[](uint32_t i);
    char& front();
    char& back();
    char* data() const;
    const char* c_str() const;

    bool empty();
    uint32_t size() const;
    uint32_t length() const;
    bool reserve(uint32_t new_string_size);
    uint32_t capacity();
    bool shrink_to_fit();

    void clear();
    bool push_back(char item);
    bool pop_back();
    bool append(const char *str);
    bool append(char *str, uint32_t str_len);
    bool append(const char *str, uint32_t str_len);
    bool append(const string& str);
    bool append(char ch);

    bool operator+=(const char *str);
    bool operator+=(const string& str);
    bool operator+=(char ch);

    bool resize(uint32_t new_str_size);
    bool resize(uint32_t new_str_size, char value);

    bool assign(const char *str);
    bool assign(char *str, uint32_t str_len);
    bool assign(const char *str, uint32_t str_len);
    bool assign(const string& str);

    bool operator==(const string & name) const;
    bool operator==(string &str) const;
    bool operator!=(const string & name) const;
    bool operator!=(string &name) const;
    bool operator<(const string & other) const;

    string operator + (string &str);
    string operator + (const char *str);

    heap_t* get_mem_pointer() const;
};

} // namespace mcustl

#endif // MCUSTL_USE_STRING

#endif // MCUSTL_STRING_H
