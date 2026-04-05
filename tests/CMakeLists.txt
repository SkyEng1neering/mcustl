cmake_minimum_required(VERSION 3.14)
project(mcustl_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Compiler flags
add_compile_options(-Wall -Wextra -g)

# Sanitizers
option(USE_SANITIZERS "Enable ASan/UBSan" ON)
if(USE_SANITIZERS AND NOT WIN32)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# mcustl allocator library (C)
add_library(mcustl_alloc STATIC ../src/mcustl_alloc.c)
target_include_directories(mcustl_alloc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(mcustl_alloc PUBLIC
    MCUSTL_CUSTOM_CONF_FILE="mcustl_test_conf.h"
)

# mcustl string library (C++)
add_library(mcustl_string STATIC ../src/mcustl_string.cpp)
target_include_directories(mcustl_string PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(mcustl_string PUBLIC mcustl_alloc)
target_compile_definitions(mcustl_string PUBLIC
    MCUSTL_CUSTOM_CONF_FILE="mcustl_test_conf.h"
)

# Test sources
set(TEST_SOURCES
    test_vector.cpp
    test_string.cpp
    test_smart_ptr.cpp
    test_list.cpp
    test_map.cpp
    test_integration.cpp
    test_advanced.cpp
)

# Test executable
add_executable(mcustl_tests ${TEST_SOURCES})
find_package(Threads REQUIRED)
target_link_libraries(mcustl_tests
    mcustl_string
    mcustl_alloc
    GTest::gtest_main
    Threads::Threads
)
target_include_directories(mcustl_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(mcustl_tests PRIVATE
    MCUSTL_CUSTOM_CONF_FILE="mcustl_test_conf.h"
)

# Enable CTest integration
include(GoogleTest)
gtest_discover_tests(mcustl_tests)
