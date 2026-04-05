/**
 * @file test_list.cpp
 * @brief mcustl::list tests
 */

#include "mcustl_test_config.h"

// ==================== Basic Operations ====================

TEST_F(McustlTestFixture, ListDefaultConstruct) {
    mcustl::list<int> lst;
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_TRUE(lst.empty());
}

TEST_F(McustlTestFixture, ListPushBack) {
    mcustl::list<int> lst;
    EXPECT_TRUE(lst.push_back(10));
    EXPECT_TRUE(lst.push_back(20));
    EXPECT_TRUE(lst.push_back(30));

    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.front(), 10);
    EXPECT_EQ(lst.back(), 30);
}

TEST_F(McustlTestFixture, ListPushFront) {
    mcustl::list<int> lst;
    EXPECT_TRUE(lst.push_front(30));
    EXPECT_TRUE(lst.push_front(20));
    EXPECT_TRUE(lst.push_front(10));

    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.front(), 10);
    EXPECT_EQ(lst.back(), 30);
}

TEST_F(McustlTestFixture, ListPopBack) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);

    EXPECT_TRUE(lst.pop_back());
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(lst.back(), 2);

    EXPECT_TRUE(lst.pop_back());
    EXPECT_TRUE(lst.pop_back());
    EXPECT_FALSE(lst.pop_back());
    EXPECT_TRUE(lst.empty());
}

TEST_F(McustlTestFixture, ListPopFront) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);

    EXPECT_TRUE(lst.pop_front());
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(lst.front(), 2);

    EXPECT_TRUE(lst.pop_front());
    EXPECT_TRUE(lst.pop_front());
    EXPECT_FALSE(lst.pop_front());
    EXPECT_TRUE(lst.empty());
}

// ==================== Element Access ====================

TEST_F(McustlTestFixture, ListFrontBack) {
    mcustl::list<int> lst;
    lst.push_back(100);
    lst.push_back(200);
    lst.push_back(300);

    EXPECT_EQ(lst.front(), 100);
    EXPECT_EQ(lst.back(), 300);
}

TEST_F(McustlTestFixture, ListFrontBackMutable) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);

    lst.front() = 10;
    lst.back() = 20;

    EXPECT_EQ(lst.front(), 10);
    EXPECT_EQ(lst.back(), 20);
}

// ==================== Capacity ====================

TEST_F(McustlTestFixture, ListSizeEmpty) {
    mcustl::list<int> lst;
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_TRUE(lst.empty());

    lst.push_back(1);
    EXPECT_EQ(lst.size(), 1u);
    EXPECT_FALSE(lst.empty());

    lst.push_back(2);
    EXPECT_EQ(lst.size(), 2u);
}

TEST_F(McustlTestFixture, ListClear) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);

    lst.clear();
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_TRUE(lst.empty());
}

TEST_F(McustlTestFixture, ListReserve) {
    mcustl::list<int> lst;
    EXPECT_TRUE(lst.reserve(10));
    EXPECT_EQ(lst.size(), 0u);

    // After reserving, should be able to push without reallocation
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(lst.push_back(i));
    }
    EXPECT_EQ(lst.size(), 10u);
}

// ==================== Remove ====================

TEST_F(McustlTestFixture, ListRemove) {
    mcustl::list<int> lst;
    lst.push_back(10);
    lst.push_back(20);
    lst.push_back(30);
    lst.push_back(40);

    EXPECT_TRUE(lst.remove(20));
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.front(), 10);

    // Verify 20 is gone by iterating
    auto it = lst.begin();
    EXPECT_EQ(*it, 10); ++it;
    EXPECT_EQ(*it, 30); ++it;
    EXPECT_EQ(*it, 40); ++it;
    EXPECT_TRUE(it == lst.end());
}

TEST_F(McustlTestFixture, ListRemoveFirst) {
    mcustl::list<int> lst;
    lst.push_back(10);
    lst.push_back(20);
    lst.push_back(30);

    EXPECT_TRUE(lst.remove(10));
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(lst.front(), 20);
}

TEST_F(McustlTestFixture, ListRemoveLast) {
    mcustl::list<int> lst;
    lst.push_back(10);
    lst.push_back(20);
    lst.push_back(30);

    EXPECT_TRUE(lst.remove(30));
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(lst.back(), 20);
}

TEST_F(McustlTestFixture, ListRemoveNonExistent) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);

    EXPECT_FALSE(lst.remove(99));
    EXPECT_EQ(lst.size(), 2u);
}

TEST_F(McustlTestFixture, ListRemoveAll) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(1);
    lst.push_back(3);
    lst.push_back(1);

    uint32_t removed = lst.remove_all(1);
    EXPECT_EQ(removed, 3u);
    EXPECT_EQ(lst.size(), 2u);

    auto it = lst.begin();
    EXPECT_EQ(*it, 2); ++it;
    EXPECT_EQ(*it, 3); ++it;
    EXPECT_TRUE(it == lst.end());
}

// ==================== Iterator ====================

TEST_F(McustlTestFixture, ListIteratorForward) {
    mcustl::list<int> lst;
    lst.push_back(10);
    lst.push_back(20);
    lst.push_back(30);
    lst.push_back(40);

    int expected[] = {10, 20, 30, 40};
    int idx = 0;
    for (auto it = lst.begin(); it != lst.end(); ++it) {
        EXPECT_EQ(*it, expected[idx++]);
    }
    EXPECT_EQ(idx, 4);
}

TEST_F(McustlTestFixture, ListIteratorEmpty) {
    mcustl::list<int> lst;
    EXPECT_TRUE(lst.begin() == lst.end());
}

TEST_F(McustlTestFixture, ListIteratorMutate) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);

    for (auto it = lst.begin(); it != lst.end(); ++it) {
        *it *= 10;
    }

    EXPECT_EQ(lst.front(), 10);
    EXPECT_EQ(lst.back(), 30);
}

// ==================== Copy/Assignment ====================

TEST_F(McustlTestFixture, ListCopy) {
    mcustl::list<int> original;
    original.push_back(1);
    original.push_back(2);
    original.push_back(3);

    mcustl::list<int> copy = original;
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_EQ(copy.front(), 1);
    EXPECT_EQ(copy.back(), 3);

    // Modify original, copy should be independent
    original.pop_front();
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_EQ(copy.front(), 1);
}

TEST_F(McustlTestFixture, ListAssignment) {
    mcustl::list<int> a;
    a.push_back(1);
    a.push_back(2);

    mcustl::list<int> b;
    b.push_back(10);
    b.push_back(20);
    b.push_back(30);

    b = a;
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b.front(), 1);
    EXPECT_EQ(b.back(), 2);
}

// ==================== Edge Cases ====================

TEST_F(McustlTestFixture, ListSingleElement) {
    mcustl::list<int> lst;
    lst.push_back(42);

    EXPECT_EQ(lst.size(), 1u);
    EXPECT_EQ(lst.front(), 42);
    EXPECT_EQ(lst.back(), 42);

    EXPECT_TRUE(lst.pop_back());
    EXPECT_TRUE(lst.empty());
}

TEST_F(McustlTestFixture, ListPushFrontAndBack) {
    mcustl::list<int> lst;
    lst.push_back(2);
    lst.push_front(1);
    lst.push_back(3);
    lst.push_front(0);

    // Expected: 0 1 2 3
    auto it = lst.begin();
    EXPECT_EQ(*it, 0); ++it;
    EXPECT_EQ(*it, 1); ++it;
    EXPECT_EQ(*it, 2); ++it;
    EXPECT_EQ(*it, 3); ++it;
    EXPECT_TRUE(it == lst.end());
}

TEST_F(McustlTestFixture, ListRemoveOnlyElement) {
    mcustl::list<int> lst;
    lst.push_back(42);

    EXPECT_TRUE(lst.remove(42));
    EXPECT_TRUE(lst.empty());
}

// ==================== TrackedObject Lifecycle ====================

TEST_F(McustlTestFixture, ListTrackedObjectLifecycle) {
    TrackedObject::resetCounters();
    {
        mcustl::list<TrackedObject> lst;
        lst.push_back(TrackedObject(10));
        lst.push_back(TrackedObject(20));
        lst.push_back(TrackedObject(30));

        EXPECT_EQ(lst.size(), 3u);
    }
    EXPECT_GT(TrackedObject::destruct_count, 0);
}

TEST_F(McustlTestFixture, ListIntegrityObject) {
    IntegrityObject::reset_counters();
    {
        mcustl::list<IntegrityObject> lst;
        for (int i = 0; i < 5; i++) {
            lst.push_back(IntegrityObject(i * 10));
        }

        // Remove from middle
        lst.remove(IntegrityObject(20));

        // Verify remaining are valid
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            EXPECT_TRUE(it->is_valid());
        }
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}

// ==================== Defrag ====================

TEST_F(McustlTestFixture, ListMultipleListsDefrag) {
    mcustl::list<int> l1;
    mcustl::list<int> l2;

    for (int i = 0; i < 5; i++) l1.push_back(i);
    for (int i = 10; i < 15; i++) l2.push_back(i);

    // Free l1, l2 should survive defrag
    l1.clear();

    EXPECT_EQ(l2.size(), 5u);
    auto it = l2.begin();
    for (int i = 10; i < 15; i++) {
        EXPECT_EQ(*it, i);
        ++it;
    }
}

// ==================== Stress ====================

TEST_F(McustlTestFixture, ListStressPushPop) {
    mcustl::list<int> lst;
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 10; i++) {
            ASSERT_TRUE(lst.push_back(round * 100 + i));
        }
        for (int i = 0; i < 5; i++) {
            ASSERT_TRUE(lst.pop_front());
        }
    }
    EXPECT_EQ(lst.size(), 25u);
}

TEST_F(McustlTestFixture, ListStressRemoveReinsert) {
    mcustl::list<int> lst;
    for (int i = 0; i < 20; i++) {
        lst.push_back(i);
    }

    // Remove every other element
    for (int i = 0; i < 20; i += 2) {
        lst.remove(i);
    }
    EXPECT_EQ(lst.size(), 10u);

    // Re-insert
    for (int i = 0; i < 10; i++) {
        lst.push_back(100 + i);
    }
    EXPECT_EQ(lst.size(), 20u);
}

// ==================== Insert at Iterator ====================

TEST_F(McustlTestFixture, ListInsertBeforeBegin) {
    mcustl::list<int> lst;
    lst.push_back(2);
    lst.push_back(3);

    auto it = lst.insert(lst.begin(), 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.front(), 1);

    auto check = lst.begin();
    EXPECT_EQ(*check, 1); ++check;
    EXPECT_EQ(*check, 2); ++check;
    EXPECT_EQ(*check, 3); ++check;
    EXPECT_TRUE(check == lst.end());
}

TEST_F(McustlTestFixture, ListInsertBeforeEnd) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);

    auto it = lst.insert(lst.end(), 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.back(), 3);

    auto check = lst.begin();
    EXPECT_EQ(*check, 1); ++check;
    EXPECT_EQ(*check, 2); ++check;
    EXPECT_EQ(*check, 3); ++check;
    EXPECT_TRUE(check == lst.end());
}

TEST_F(McustlTestFixture, ListInsertMiddle) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(3);

    auto it = lst.begin();
    ++it; // points to 3
    auto inserted = lst.insert(it, 2);

    EXPECT_EQ(*inserted, 2);
    EXPECT_EQ(lst.size(), 3u);

    auto check = lst.begin();
    EXPECT_EQ(*check, 1); ++check;
    EXPECT_EQ(*check, 2); ++check;
    EXPECT_EQ(*check, 3); ++check;
    EXPECT_TRUE(check == lst.end());
}

TEST_F(McustlTestFixture, ListInsertIntoEmpty) {
    mcustl::list<int> lst;
    auto it = lst.insert(lst.end(), 42);
    EXPECT_EQ(*it, 42);
    EXPECT_EQ(lst.size(), 1u);
    EXPECT_EQ(lst.front(), 42);
    EXPECT_EQ(lst.back(), 42);
}

TEST_F(McustlTestFixture, ListInsertMoveValue) {
    mcustl::list<mcustl::string> lst;
    lst.push_back(mcustl::string("a"));
    lst.push_back(mcustl::string("c"));

    auto it = lst.begin();
    ++it; // points to "c"
    mcustl::string b("b");
    lst.insert(it, static_cast<mcustl::string&&>(b));

    EXPECT_EQ(lst.size(), 3u);
    auto check = lst.begin();
    EXPECT_STREQ((*check).c_str(), "a"); ++check;
    EXPECT_STREQ((*check).c_str(), "b"); ++check;
    EXPECT_STREQ((*check).c_str(), "c"); ++check;
    EXPECT_TRUE(check == lst.end());
}

// ==================== Erase at Iterator ====================

TEST_F(McustlTestFixture, ListEraseBegin) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);

    auto it = lst.erase(lst.begin());
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(lst.front(), 2);
}

TEST_F(McustlTestFixture, ListEraseMiddle) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);

    auto it = lst.begin();
    ++it; // points to 2
    auto next = lst.erase(it);

    EXPECT_EQ(*next, 3);
    EXPECT_EQ(lst.size(), 2u);

    auto check = lst.begin();
    EXPECT_EQ(*check, 1); ++check;
    EXPECT_EQ(*check, 3); ++check;
    EXPECT_TRUE(check == lst.end());
}

TEST_F(McustlTestFixture, ListEraseLast) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);

    auto it = lst.begin();
    ++it; ++it; // points to 3
    auto next = lst.erase(it);

    EXPECT_TRUE(next == lst.end());
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(lst.back(), 2);
}

TEST_F(McustlTestFixture, ListEraseOnly) {
    mcustl::list<int> lst;
    lst.push_back(42);

    auto it = lst.erase(lst.begin());
    EXPECT_TRUE(it == lst.end());
    EXPECT_TRUE(lst.empty());
}

// ==================== Bidirectional Iterator ====================

TEST_F(McustlTestFixture, ListIteratorBidirectional) {
    mcustl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);
    lst.push_back(4);

    auto it = lst.begin();
    ++it; ++it; ++it; // at 4
    EXPECT_EQ(*it, 4);

    --it; EXPECT_EQ(*it, 3);
    --it; EXPECT_EQ(*it, 2);
    --it; EXPECT_EQ(*it, 1);
    EXPECT_TRUE(it == lst.begin());
}

TEST_F(McustlTestFixture, ListIteratorDecrementFromEnd) {
    mcustl::list<int> lst;
    lst.push_back(10);
    lst.push_back(20);
    lst.push_back(30);

    auto it = lst.end();
    --it; EXPECT_EQ(*it, 30);
    --it; EXPECT_EQ(*it, 20);
    --it; EXPECT_EQ(*it, 10);
    EXPECT_TRUE(it == lst.begin());
}

// ==================== Combined Insert/Erase ====================

TEST_F(McustlTestFixture, ListInsertEraseChained) {
    mcustl::list<int> lst;
    for (int i = 1; i <= 5; i++) lst.push_back(i);

    // Erase 3 (middle)
    auto it = lst.begin();
    ++it; ++it; // points to 3
    it = lst.erase(it); // returns iterator to 4
    EXPECT_EQ(*it, 4);

    // Insert 30 before 4
    it = lst.insert(it, 30);
    EXPECT_EQ(*it, 30);

    // Expected: 1, 2, 30, 4, 5
    EXPECT_EQ(lst.size(), 5u);
    auto check = lst.begin();
    EXPECT_EQ(*check, 1); ++check;
    EXPECT_EQ(*check, 2); ++check;
    EXPECT_EQ(*check, 30); ++check;
    EXPECT_EQ(*check, 4); ++check;
    EXPECT_EQ(*check, 5); ++check;
    EXPECT_TRUE(check == lst.end());
}

// ==================== Range-based for ====================

TEST_F(McustlTestFixture, ListRangeFor) {
    mcustl::list<int> lst;
    for (int i = 0; i < 5; i++) lst.push_back(i * 10);

    int sum = 0;
    for (auto& x : lst) sum += x;
    EXPECT_EQ(sum, 100);  // 0+10+20+30+40
}

TEST_F(McustlTestFixture, ListRangeForConst) {
    mcustl::list<int> lst;
    for (int i = 1; i <= 4; i++) lst.push_back(i);

    const auto& clst = lst;
    int product = 1;
    for (const auto& x : clst) product *= x;
    EXPECT_EQ(product, 24);
}

TEST_F(McustlTestFixture, ListRangeForMutate) {
    mcustl::list<int> lst;
    for (int i = 0; i < 3; i++) lst.push_back(i);

    for (auto& x : lst) x += 100;

    auto it = lst.begin();
    EXPECT_EQ(*it, 100); ++it;
    EXPECT_EQ(*it, 101); ++it;
    EXPECT_EQ(*it, 102);
}

TEST_F(McustlTestFixture, ListRangeForEmpty) {
    mcustl::list<int> lst;
    int count = 0;
    for (auto& x : lst) { (void)x; count++; }
    EXPECT_EQ(count, 0);
}

// ==================== Return from Function ====================

static mcustl::list<int> make_int_list(int n) {
    mcustl::list<int> lst;
    for (int i = 0; i < n; i++) lst.push_back(i * 10);
    return lst;
}

TEST_F(McustlTestFixture, ListReturnFromFunction) {
    mcustl::list<int> lst = make_int_list(5);
    EXPECT_EQ(lst.size(), 5u);

    auto it = lst.begin();
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(*it, i * 10);
        ++it;
    }
}

TEST_F(McustlTestFixture, ListReturnAndModify) {
    mcustl::list<int> lst = make_int_list(3);
    lst.push_back(100);
    lst.push_front(-1);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(lst.front(), -1);
    EXPECT_EQ(lst.back(), 100);
}

static mcustl::list<mcustl::string> make_string_list() {
    mcustl::list<mcustl::string> lst;
    lst.push_back(mcustl::string("one"));
    lst.push_back(mcustl::string("two"));
    lst.push_back(mcustl::string("three"));
    return lst;
}

TEST_F(McustlTestFixture, ListReturnOfStrings) {
    mcustl::list<mcustl::string> lst = make_string_list();
    EXPECT_EQ(lst.size(), 3u);

    auto it = lst.begin();
    EXPECT_STREQ((*it).c_str(), "one"); ++it;
    EXPECT_STREQ((*it).c_str(), "two"); ++it;
    EXPECT_STREQ((*it).c_str(), "three");
}

static mcustl::list<IntegrityObject> make_integrity_list(int n) {
    mcustl::list<IntegrityObject> lst;
    for (int i = 0; i < n; i++) lst.push_back(IntegrityObject(i));
    return lst;
}

TEST_F(McustlTestFixture, ListReturnIntegrity) {
    IntegrityObject::reset_counters();
    {
        mcustl::list<IntegrityObject> lst = make_integrity_list(5);
        EXPECT_EQ(lst.size(), 5u);
        for (auto& obj : lst) {
            EXPECT_TRUE(obj.is_valid());
        }
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}

TEST_F(McustlTestFixture, ListReturnAssign) {
    mcustl::list<int> lst;
    lst.push_back(999);
    lst = make_int_list(4);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(lst.front(), 0);
    EXPECT_EQ(lst.back(), 30);
}

// Conditional return prevents NRVO — forces move constructor
static mcustl::list<int> make_list_conditional(bool flag) {
    mcustl::list<int> a;
    a.push_back(1);
    a.push_back(2);
    mcustl::list<int> b;
    b.push_back(10);
    b.push_back(20);
    b.push_back(30);
    if (flag) return a;
    return b;
}

TEST_F(McustlTestFixture, ListReturnConditionalTrue) {
    mcustl::list<int> lst = make_list_conditional(true);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(lst.front(), 1);
    EXPECT_EQ(lst.back(), 2);
    lst.push_back(3);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.back(), 3);
}

TEST_F(McustlTestFixture, ListReturnConditionalFalse) {
    mcustl::list<int> lst = make_list_conditional(false);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.front(), 10);
    EXPECT_EQ(lst.back(), 30);
    lst.push_front(0);
    EXPECT_EQ(lst.front(), 0);
}

static mcustl::list<IntegrityObject> make_integrity_list_conditional(bool flag) {
    mcustl::list<IntegrityObject> a;
    a.push_back(IntegrityObject(1));
    mcustl::list<IntegrityObject> b;
    b.push_back(IntegrityObject(10));
    b.push_back(IntegrityObject(20));
    if (flag) return a;
    return b;
}

TEST_F(McustlTestFixture, ListReturnConditionalIntegrity) {
    IntegrityObject::reset_counters();
    {
        mcustl::list<IntegrityObject> lst = make_integrity_list_conditional(false);
        for (auto& obj : lst) {
            EXPECT_TRUE(obj.is_valid());
        }
        lst.push_back(IntegrityObject(99));
        EXPECT_TRUE(lst.back().is_valid());
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}
