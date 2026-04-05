/**
 * @file test_vector.cpp
 * @brief mcustl::vector tests
 */

#include "mcustl_test_config.h"

// ==================== Basic Operations ====================

TEST_F(McustlTestFixture, VectorDefaultConstruct) {
    mcustl::vector<int> vec;
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.capacity(), 0u);
}

TEST_F(McustlTestFixture, VectorPushBack) {
    mcustl::vector<int> vec;
    EXPECT_TRUE(vec.push_back(10));
    EXPECT_TRUE(vec.push_back(20));
    EXPECT_TRUE(vec.push_back(30));

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 30);
}

TEST_F(McustlTestFixture, VectorPopBack) {
    mcustl::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    EXPECT_TRUE(vec.pop_back());
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec.back(), 2);

    EXPECT_TRUE(vec.pop_back());
    EXPECT_TRUE(vec.pop_back());
    EXPECT_FALSE(vec.pop_back());
    EXPECT_TRUE(vec.empty());
}

TEST_F(McustlTestFixture, VectorPopMiddle) {
    mcustl::vector<int> vec;
    for (int i = 0; i < 5; i++) vec.push_back(i * 10);

    EXPECT_TRUE(vec.pop(2));  // Remove 20
    EXPECT_EQ(vec.size(), 4u);
    EXPECT_EQ(vec[0], 0);
    EXPECT_EQ(vec[1], 10);
    EXPECT_EQ(vec[2], 30);
    EXPECT_EQ(vec[3], 40);
}

TEST_F(McustlTestFixture, VectorReserve) {
    mcustl::vector<int> vec;
    EXPECT_TRUE(vec.reserve(10));
    EXPECT_GE(vec.capacity(), 10u);
    EXPECT_EQ(vec.size(), 0u);
}

TEST_F(McustlTestFixture, VectorResize) {
    mcustl::vector<int> vec;
    EXPECT_TRUE(vec.resize(5, 42));
    EXPECT_EQ(vec.size(), 5u);
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(vec[i], 42);
    }

    EXPECT_TRUE(vec.resize(3));
    EXPECT_EQ(vec.size(), 3u);
}

TEST_F(McustlTestFixture, VectorClear) {
    mcustl::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.clear();
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.empty());
    EXPECT_GT(vec.capacity(), 0u);
}

TEST_F(McustlTestFixture, VectorCopy) {
    mcustl::vector<int> vec;
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    mcustl::vector<int> copy = vec;
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_EQ(copy[0], 10);
    EXPECT_EQ(copy[1], 20);
    EXPECT_EQ(copy[2], 30);

    // Modify original, copy should be independent
    vec[0] = 99;
    EXPECT_EQ(copy[0], 10);
}

TEST_F(McustlTestFixture, VectorShrinkToFit) {
    mcustl::vector<int> vec;
    vec.reserve(100);
    vec.push_back(1);
    vec.push_back(2);

    EXPECT_GT(vec.capacity(), vec.size());
    EXPECT_TRUE(vec.shrink_to_fit());
    EXPECT_EQ(vec.capacity(), vec.size());
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
}

TEST_F(McustlTestFixture, VectorFrontBack) {
    mcustl::vector<int> vec;
    vec.push_back(100);
    vec.push_back(200);
    vec.push_back(300);

    EXPECT_EQ(vec.front(), 100);
    EXPECT_EQ(vec.back(), 300);
}

TEST_F(McustlTestFixture, VectorData) {
    mcustl::vector<int> vec;
    vec.push_back(5);
    vec.push_back(10);

    int* d = vec.data();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d[0], 5);
    EXPECT_EQ(d[1], 10);
}

// ==================== TrackedObject Lifecycle ====================

TEST_F(McustlTestFixture, VectorTrackedObjectLifecycle) {
    TrackedObject::resetCounters();
    {
        mcustl::vector<TrackedObject> vec;
        vec.push_back(TrackedObject(10));
        vec.push_back(TrackedObject(20));
        vec.push_back(TrackedObject(30));

        EXPECT_EQ(vec.size(), 3u);
        EXPECT_EQ(vec[0].value, 10);
        EXPECT_EQ(vec[1].value, 20);
        EXPECT_EQ(vec[2].value, 30);
    }
    // After vector destroyed, all destructs should have been called
    EXPECT_GT(TrackedObject::destruct_count, 0);
}

// ==================== IntegrityObject Tests ====================

TEST_F(McustlTestFixture, VectorIntegrityObject) {
    IntegrityObject::reset_counters();
    {
        mcustl::vector<IntegrityObject> vec;
        for (int i = 0; i < 5; i++) {
            vec.push_back(IntegrityObject(i * 10));
        }

        for (uint32_t i = 0; i < vec.size(); i++) {
            EXPECT_TRUE(vec[i].is_valid());
            EXPECT_EQ(vec[i].value, (int)(i * 10));
        }

        vec.pop(2);  // Remove middle element
        EXPECT_EQ(vec.size(), 4u);
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}

// ==================== Multiple Vectors ====================

TEST_F(McustlTestFixture, VectorMultipleVectors) {
    mcustl::vector<int> v1;
    mcustl::vector<int> v2;

    for (int i = 0; i < 10; i++) v1.push_back(i);
    for (int i = 10; i < 20; i++) v2.push_back(i);

    EXPECT_EQ(v1.size(), 10u);
    EXPECT_EQ(v2.size(), 10u);

    // Free v1, v2 should be unaffected (defrag test)
    v1.clear();
    v1.shrink_to_fit();

    EXPECT_EQ(v2.size(), 10u);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(v2[i], i + 10);
    }
}

// ==================== Stress ====================

TEST_F(McustlTestFixture, VectorStressPushPop) {
    mcustl::vector<int> vec;
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 20; i++) {
            ASSERT_TRUE(vec.push_back(round * 100 + i));
        }
        for (int i = 0; i < 10; i++) {
            ASSERT_TRUE(vec.pop_back());
        }
    }
    EXPECT_EQ(vec.size(), 50u);
}

// ==================== Range-based for ====================

TEST_F(McustlTestFixture, VectorRangeFor) {
    mcustl::vector<int> vec;
    for (int i = 0; i < 5; i++) vec.push_back(i * 10);

    int sum = 0;
    for (auto& x : vec) sum += x;
    EXPECT_EQ(sum, 100);  // 0+10+20+30+40
}

TEST_F(McustlTestFixture, VectorRangeForConst) {
    mcustl::vector<int> vec;
    for (int i = 1; i <= 4; i++) vec.push_back(i);

    const auto& cvec = vec;
    int product = 1;
    for (const auto& x : cvec) product *= x;
    EXPECT_EQ(product, 24);  // 1*2*3*4
}

TEST_F(McustlTestFixture, VectorRangeForMutate) {
    mcustl::vector<int> vec;
    for (int i = 0; i < 3; i++) vec.push_back(i);

    for (auto& x : vec) x += 100;

    EXPECT_EQ(vec[0], 100);
    EXPECT_EQ(vec[1], 101);
    EXPECT_EQ(vec[2], 102);
}

TEST_F(McustlTestFixture, VectorRangeForEmpty) {
    mcustl::vector<int> vec;
    int count = 0;
    for (auto& x : vec) { (void)x; count++; }
    EXPECT_EQ(count, 0);
}

TEST_F(McustlTestFixture, VectorRangeForStrings) {
    mcustl::vector<mcustl::string> vec;
    vec.push_back(mcustl::string("hello"));
    vec.push_back(mcustl::string("world"));

    mcustl::string result;
    for (auto& s : vec) {
        result += s;
    }
    EXPECT_STREQ(result.c_str(), "helloworld");
}

// ==================== Return from Function ====================

static mcustl::vector<int> make_int_vector(int n) {
    mcustl::vector<int> v;
    for (int i = 0; i < n; i++) v.push_back(i * 10);
    return v;
}

TEST_F(McustlTestFixture, VectorReturnFromFunction) {
    mcustl::vector<int> vec = make_int_vector(5);
    EXPECT_EQ(vec.size(), 5u);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(vec[i], i * 10);
    }
}

TEST_F(McustlTestFixture, VectorReturnAndModify) {
    mcustl::vector<int> vec = make_int_vector(3);
    vec.push_back(100);
    vec.push_back(200);
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(vec[3], 100);
    EXPECT_EQ(vec[4], 200);
}

static mcustl::vector<mcustl::string> make_string_vector() {
    mcustl::vector<mcustl::string> v;
    v.push_back(mcustl::string("alpha"));
    v.push_back(mcustl::string("beta"));
    v.push_back(mcustl::string("gamma"));
    return v;
}

TEST_F(McustlTestFixture, VectorReturnOfStrings) {
    mcustl::vector<mcustl::string> vec = make_string_vector();
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_STREQ(vec[0].c_str(), "alpha");
    EXPECT_STREQ(vec[1].c_str(), "beta");
    EXPECT_STREQ(vec[2].c_str(), "gamma");
}

static mcustl::vector<IntegrityObject> make_integrity_vector(int n) {
    mcustl::vector<IntegrityObject> v;
    for (int i = 0; i < n; i++) v.push_back(IntegrityObject(i));
    return v;
}

TEST_F(McustlTestFixture, VectorReturnIntegrity) {
    IntegrityObject::reset_counters();
    {
        mcustl::vector<IntegrityObject> vec = make_integrity_vector(5);
        EXPECT_EQ(vec.size(), 5u);
        for (uint32_t i = 0; i < vec.size(); i++) {
            EXPECT_TRUE(vec[i].is_valid());
            EXPECT_EQ(vec[i].value, (int)i);
        }
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}

static mcustl::vector<int> make_vector_chain(int depth) {
    if (depth <= 1) return make_int_vector(3);
    mcustl::vector<int> inner = make_vector_chain(depth - 1);
    inner.push_back(depth * 100);
    return inner;
}

TEST_F(McustlTestFixture, VectorReturnNested) {
    mcustl::vector<int> vec = make_vector_chain(3);
    // depth=1: [0, 10, 20], depth=2: +200, depth=3: +300
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(vec[0], 0);
    EXPECT_EQ(vec[1], 10);
    EXPECT_EQ(vec[2], 20);
    EXPECT_EQ(vec[3], 200);
    EXPECT_EQ(vec[4], 300);
}

TEST_F(McustlTestFixture, VectorReturnAssign) {
    mcustl::vector<int> vec;
    vec.push_back(999);
    vec = make_int_vector(4);
    EXPECT_EQ(vec.size(), 4u);
    EXPECT_EQ(vec[0], 0);
    EXPECT_EQ(vec[3], 30);
}

// Conditional return prevents NRVO — forces move constructor
static mcustl::vector<int> make_vector_conditional(bool flag) {
    mcustl::vector<int> a;
    a.push_back(1);
    a.push_back(2);
    mcustl::vector<int> b;
    b.push_back(10);
    b.push_back(20);
    b.push_back(30);
    if (flag) return a;
    return b;
}

TEST_F(McustlTestFixture, VectorReturnConditionalTrue) {
    mcustl::vector<int> vec = make_vector_conditional(true);
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    // Verify container still works after move
    vec.push_back(3);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[2], 3);
}

TEST_F(McustlTestFixture, VectorReturnConditionalFalse) {
    mcustl::vector<int> vec = make_vector_conditional(false);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 30);
    vec.push_back(40);
    EXPECT_EQ(vec.size(), 4u);
}

static mcustl::vector<IntegrityObject> make_integrity_vector_conditional(bool flag) {
    mcustl::vector<IntegrityObject> a;
    a.push_back(IntegrityObject(1));
    a.push_back(IntegrityObject(2));
    mcustl::vector<IntegrityObject> b;
    b.push_back(IntegrityObject(10));
    b.push_back(IntegrityObject(20));
    b.push_back(IntegrityObject(30));
    if (flag) return a;
    return b;
}

TEST_F(McustlTestFixture, VectorReturnConditionalIntegrity) {
    IntegrityObject::reset_counters();
    {
        mcustl::vector<IntegrityObject> vec = make_integrity_vector_conditional(true);
        for (uint32_t i = 0; i < vec.size(); i++) {
            EXPECT_TRUE(vec[i].is_valid());
        }
        vec.push_back(IntegrityObject(99));
        EXPECT_TRUE(vec.back().is_valid());
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}
