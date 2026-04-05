/**
 * @file test_smart_ptr.cpp
 * @brief mcustl::smart_ptr tests
 */

#include "mcustl_test_config.h"

// ==================== Basic Operations ====================

TEST_F(McustlTestFixture, SmartPtrDefaultNull) {
    mcustl::smart_ptr<int> p;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(static_cast<bool>(p));
    EXPECT_EQ(p.allocated_size(), 0u);
}

TEST_F(McustlTestFixture, SmartPtrAllocate) {
    mcustl::smart_ptr<int> p;
    EXPECT_TRUE(p.allocate(1));
    EXPECT_NE(p.get(), nullptr);
    *p = 42;
    EXPECT_EQ(*p, 42);
}

TEST_F(McustlTestFixture, SmartPtrAllocateArray) {
    mcustl::smart_ptr<int> p;
    EXPECT_TRUE(p.allocate(10));
    EXPECT_EQ(p.allocated_size(), 10u);
    for (uint32_t i = 0; i < 10; i++) {
        p[i] = (int)(i * 5);
    }
    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_EQ(p[i], (int)(i * 5));
    }
}

TEST_F(McustlTestFixture, SmartPtrCreate) {
    TrackedObject::resetCounters();
    {
        mcustl::smart_ptr<TrackedObject> p;
        EXPECT_TRUE(p.create(42));
        EXPECT_EQ(p->value, 42);
        EXPECT_EQ(p.constructed_count(), 1u);
    }
    EXPECT_GT(TrackedObject::destruct_count, 0);
}

TEST_F(McustlTestFixture, SmartPtrCreateArray) {
    TrackedObject::resetCounters();
    {
        mcustl::smart_ptr<TrackedObject> p;
        EXPECT_TRUE(p.create_array(5));
        EXPECT_EQ(p.constructed_count(), 5u);
        for (uint32_t i = 0; i < 5; i++) {
            p[i].value = (int)(i * 10);
        }
    }
    EXPECT_GT(TrackedObject::destruct_count, 0);
}

TEST_F(McustlTestFixture, SmartPtrReset) {
    mcustl::smart_ptr<int> p;
    p.allocate(1);
    *p = 42;
    p.reset();
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(static_cast<bool>(p));
}

// ==================== Move Semantics ====================

TEST_F(McustlTestFixture, SmartPtrMoveConstruct) {
    mcustl::smart_ptr<int> p1;
    p1.allocate(1);
    *p1 = 42;

    mcustl::smart_ptr<int> p2(static_cast<mcustl::smart_ptr<int>&&>(p1));
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_NE(p2.get(), nullptr);
    EXPECT_EQ(*p2, 42);
}

TEST_F(McustlTestFixture, SmartPtrMoveAssign) {
    mcustl::smart_ptr<int> p1;
    p1.allocate(1);
    *p1 = 42;

    mcustl::smart_ptr<int> p2;
    p2 = static_cast<mcustl::smart_ptr<int>&&>(p1);
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(*p2, 42);
}

TEST_F(McustlTestFixture, SmartPtrNullptrAssign) {
    mcustl::smart_ptr<int> p;
    p.allocate(1);
    *p = 42;
    p = nullptr;
    EXPECT_EQ(p.get(), nullptr);
}

// ==================== Swap ====================

TEST_F(McustlTestFixture, SmartPtrSwap) {
    mcustl::smart_ptr<int> p1;
    mcustl::smart_ptr<int> p2;
    p1.allocate(1); *p1 = 10;
    p2.allocate(1); *p2 = 20;

    p1.swap(p2);
    EXPECT_EQ(*p1, 20);
    EXPECT_EQ(*p2, 10);
}

// ==================== Observer Ptr ====================

TEST_F(McustlTestFixture, ObserverPtrBasic) {
    mcustl::smart_ptr<int> owner;
    owner.allocate(1);
    *owner = 42;

    mcustl::observer_ptr<int> obs = owner.get_observer();
    EXPECT_EQ(*obs, 42);
    EXPECT_EQ(obs.get(), owner.get());
}

TEST_F(McustlTestFixture, ObserverPtrNull) {
    mcustl::observer_ptr<int> obs;
    EXPECT_EQ(obs.get(), nullptr);
    EXPECT_FALSE(static_cast<bool>(obs));
}

// ==================== Factory Functions ====================

TEST_F(McustlTestFixture, MakeSmart) {
    auto p = mcustl::make_smart<TrackedObject>(42);
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(p->value, 42);
}

TEST_F(McustlTestFixture, MakeSmartArray) {
    auto arr = mcustl::make_smart_array<int>(10);
    EXPECT_NE(arr.get(), nullptr);
    EXPECT_EQ(arr.allocated_size(), 10u);
    arr[0] = 100;
    EXPECT_EQ(arr[0], 100);
}

// ==================== Double Allocate Guard ====================

TEST_F(McustlTestFixture, SmartPtrDoubleAllocateFails) {
    mcustl::smart_ptr<int> p;
    EXPECT_TRUE(p.allocate(1));
    EXPECT_FALSE(p.allocate(1));  // Already allocated
}

// ==================== Multiple Pointers Defrag ====================

TEST_F(McustlTestFixture, SmartPtrDefragSurvival) {
    mcustl::smart_ptr<int> p1;
    mcustl::smart_ptr<int> p2;
    mcustl::smart_ptr<int> p3;

    p1.allocate(1); *p1 = 10;
    p2.allocate(1); *p2 = 20;
    p3.allocate(1); *p3 = 30;

    p2.reset();  // Free middle, triggers defrag

    EXPECT_EQ(*p1, 10);
    EXPECT_EQ(*p3, 30);
}

// ==================== Array Specialization ====================

TEST_F(McustlTestFixture, SmartPtrArraySpec) {
    mcustl::smart_ptr<int[]> arr;
    EXPECT_TRUE(arr.allocate(5));
    EXPECT_EQ(arr.allocated_size(), 5u);

    for (uint32_t i = 0; i < 5; i++) {
        arr[i] = (int)(i * 100);
    }
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(arr[i], (int)(i * 100));
    }
}

TEST_F(McustlTestFixture, SmartPtrArrayMove) {
    mcustl::smart_ptr<int[]> a1;
    a1.allocate(3);
    a1[0] = 10; a1[1] = 20; a1[2] = 30;

    mcustl::smart_ptr<int[]> a2(static_cast<mcustl::smart_ptr<int[]>&&>(a1));
    EXPECT_EQ(a1.get(), nullptr);
    EXPECT_EQ(a2[0], 10);
    EXPECT_EQ(a2[1], 20);
    EXPECT_EQ(a2[2], 30);
}

// ==================== Return from Function ====================

static mcustl::smart_ptr<int> make_int_ptr(int val) {
    mcustl::smart_ptr<int> p;
    p.create(val);
    return p;
}

TEST_F(McustlTestFixture, SmartPtrReturnFromFunction) {
    mcustl::smart_ptr<int> p = make_int_ptr(42);
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(*p, 42);
}

TEST_F(McustlTestFixture, SmartPtrReturnAndModify) {
    mcustl::smart_ptr<int> p = make_int_ptr(10);
    *p = 99;
    EXPECT_EQ(*p, 99);
}

static mcustl::smart_ptr<mcustl::vector<int>> make_vector_ptr() {
    mcustl::smart_ptr<mcustl::vector<int>> p;
    p.create();
    p->push_back(1);
    p->push_back(2);
    p->push_back(3);
    return p;
}

TEST_F(McustlTestFixture, SmartPtrReturnContainerInside) {
    mcustl::smart_ptr<mcustl::vector<int>> p = make_vector_ptr();
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(p->size(), 3u);
    EXPECT_EQ((*p)[0], 1);
    EXPECT_EQ((*p)[1], 2);
    EXPECT_EQ((*p)[2], 3);
}

static mcustl::smart_ptr<TrackedObject> make_tracked_ptr(int val) {
    mcustl::smart_ptr<TrackedObject> p;
    p.create(val);
    return p;
}

TEST_F(McustlTestFixture, SmartPtrReturnTracked) {
    TrackedObject::resetCounters();
    {
        mcustl::smart_ptr<TrackedObject> p = make_tracked_ptr(77);
        EXPECT_NE(p.get(), nullptr);
        EXPECT_EQ(p->value, 77);
    }
    EXPECT_GT(TrackedObject::destruct_count, 0);
}

TEST_F(McustlTestFixture, SmartPtrReturnAssign) {
    mcustl::smart_ptr<int> p;
    p.create(1);
    p = make_int_ptr(42);
    EXPECT_EQ(*p, 42);
}

static mcustl::smart_ptr<int[]> make_array_ptr(int n) {
    mcustl::smart_ptr<int[]> p;
    p.allocate(n);
    for (int i = 0; i < n; i++) p[i] = i * 5;
    return p;
}

TEST_F(McustlTestFixture, SmartPtrArrayReturnFromFunction) {
    mcustl::smart_ptr<int[]> arr = make_array_ptr(5);
    EXPECT_NE(arr.get(), nullptr);
    EXPECT_EQ(arr.allocated_size(), 5u);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(arr[i], i * 5);
    }
}

// Conditional return prevents NRVO — forces move constructor
static mcustl::smart_ptr<int> make_ptr_conditional(bool flag) {
    mcustl::smart_ptr<int> a;
    a.create(42);
    mcustl::smart_ptr<int> b;
    b.create(99);
    if (flag) return static_cast<mcustl::smart_ptr<int>&&>(a);
    return static_cast<mcustl::smart_ptr<int>&&>(b);
}

TEST_F(McustlTestFixture, SmartPtrReturnConditionalTrue) {
    mcustl::smart_ptr<int> p = make_ptr_conditional(true);
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(*p, 42);
}

TEST_F(McustlTestFixture, SmartPtrReturnConditionalFalse) {
    mcustl::smart_ptr<int> p = make_ptr_conditional(false);
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(*p, 99);
}

static mcustl::smart_ptr<mcustl::string> make_string_ptr_conditional(bool flag) {
    mcustl::smart_ptr<mcustl::string> a;
    a.create("hello");
    mcustl::smart_ptr<mcustl::string> b;
    b.create("world");
    if (flag) return static_cast<mcustl::smart_ptr<mcustl::string>&&>(a);
    return static_cast<mcustl::smart_ptr<mcustl::string>&&>(b);
}

TEST_F(McustlTestFixture, SmartPtrReturnConditionalContainer) {
    mcustl::smart_ptr<mcustl::string> p = make_string_ptr_conditional(true);
    EXPECT_NE(p.get(), nullptr);
    EXPECT_STREQ(p->c_str(), "hello");
    *p += " world";
    EXPECT_STREQ(p->c_str(), "hello world");
}
