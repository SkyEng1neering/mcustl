/**
 * @file mcustl_test_config.h
 * @brief Test fixtures and helpers for mcustl unit tests
 */

#ifndef MCUSTL_TEST_CONFIG_H
#define MCUSTL_TEST_CONFIG_H

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

#include "mcustl.h"

static constexpr size_t TEST_HEAP_SIZE = 512 * 1024;
static constexpr size_t CANARY_SIZE = 64;
static constexpr uint8_t CANARY_PATTERN = 0xCD;

/**
 * @brief Base test fixture with fresh single-heap for each test
 */
class McustlTestFixture : public ::testing::Test {
protected:
    uint8_t canary_before[CANARY_SIZE];
    alignas(8) uint8_t heap_memory[TEST_HEAP_SIZE];
    uint8_t canary_after[CANARY_SIZE];

    void SetUp() override {
        std::memset(canary_before, CANARY_PATTERN, CANARY_SIZE);
        std::memset(canary_after, CANARY_PATTERN, CANARY_SIZE);

        mcustl_unregister_heap(true);
        mcustl_register_heap(heap_memory, TEST_HEAP_SIZE);
        ASSERT_NE(mcustl_get_default_heap(), nullptr);
    }

    void TearDown() override {
        mcustl_unregister_heap(true);

        for (size_t i = 0; i < CANARY_SIZE; i++) {
            EXPECT_EQ(canary_before[i], CANARY_PATTERN)
                << "Buffer UNDERFLOW at canary_before[" << i << "]";
            EXPECT_EQ(canary_after[i], CANARY_PATTERN)
                << "Buffer OVERFLOW at canary_after[" << i << "]";
        }
    }
};

/**
 * @brief TrackedObject for lifecycle verification
 */
class TrackedObject {
public:
    static int construct_count;
    static int destruct_count;
    static int copy_count;
    static int move_count;

    int value;

    TrackedObject() : value(0) { construct_count++; }
    explicit TrackedObject(int v) : value(v) { construct_count++; }
    TrackedObject(const TrackedObject& other) : value(other.value) { copy_count++; construct_count++; }
    TrackedObject(TrackedObject&& other) noexcept : value(other.value) { other.value = -1; move_count++; }
    ~TrackedObject() { destruct_count++; }

    TrackedObject& operator=(const TrackedObject& other) {
        value = other.value;
        return *this;
    }
    TrackedObject& operator=(TrackedObject&& other) noexcept {
        value = other.value;
        other.value = -1;
        return *this;
    }

    bool operator==(const TrackedObject& other) const { return value == other.value; }
    bool operator!=(const TrackedObject& other) const { return value != other.value; }

    static void resetCounters() {
        construct_count = 0;
        destruct_count = 0;
        copy_count = 0;
        move_count = 0;
    }
};

inline int TrackedObject::construct_count = 0;
inline int TrackedObject::destruct_count = 0;
inline int TrackedObject::copy_count = 0;
inline int TrackedObject::move_count = 0;

/**
 * @brief IntegrityObject for corruption detection
 */
class IntegrityObject {
public:
    static constexpr uint32_t ALIVE_MAGIC = 0xCAFEBABE;
    static constexpr uint32_t DEAD_MAGIC = 0xDEADBEEF;

    static int construction_count;
    static int destruction_count;
    static int double_destruction_count;

    uint32_t magic_start;
    int id;
    int value;
    uint32_t magic_end;

    static int next_id;

    IntegrityObject() : magic_start(ALIVE_MAGIC), value(0), magic_end(ALIVE_MAGIC) {
        id = next_id++;
        construction_count++;
    }

    IntegrityObject(int v) : magic_start(ALIVE_MAGIC), value(v), magic_end(ALIVE_MAGIC) {
        id = next_id++;
        construction_count++;
    }

    IntegrityObject(const IntegrityObject& other)
        : magic_start(ALIVE_MAGIC), value(other.value), magic_end(ALIVE_MAGIC) {
        EXPECT_TRUE(other.is_valid()) << "Copy from corrupted object, id=" << other.id;
        id = next_id++;
        construction_count++;
    }

    ~IntegrityObject() {
        if (magic_start != ALIVE_MAGIC || magic_end != ALIVE_MAGIC) {
            if (magic_start == DEAD_MAGIC) {
                double_destruction_count++;
                ADD_FAILURE() << "Double destruction! id=" << id;
            } else {
                ADD_FAILURE() << "Corrupted destruction! id=" << id;
            }
            return;
        }
        magic_start = DEAD_MAGIC;
        magic_end = DEAD_MAGIC;
        destruction_count++;
    }

    IntegrityObject& operator=(const IntegrityObject& other) {
        if (this != &other) {
            EXPECT_TRUE(is_valid()) << "Assignment to corrupted object";
            EXPECT_TRUE(other.is_valid()) << "Assignment from corrupted object";
            value = other.value;
        }
        return *this;
    }

    bool is_valid() const {
        return magic_start == ALIVE_MAGIC && magic_end == ALIVE_MAGIC;
    }

    bool operator==(const IntegrityObject& other) const { return value == other.value; }

    static void reset_counters() {
        construction_count = 0;
        destruction_count = 0;
        double_destruction_count = 0;
        next_id = 0;
    }

    static bool no_errors() {
        return double_destruction_count == 0;
    }
};

inline int IntegrityObject::construction_count = 0;
inline int IntegrityObject::destruction_count = 0;
inline int IntegrityObject::double_destruction_count = 0;
inline int IntegrityObject::next_id = 0;

#endif // MCUSTL_TEST_CONFIG_H
