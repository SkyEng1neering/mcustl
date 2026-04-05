/**
 * @file test_map.cpp
 * @brief mcustl::map tests
 */

#include "mcustl_test_config.h"

// ==================== Basic Operations ====================

TEST_F(McustlTestFixture, MapDefaultConstruct) {
    mcustl::map<int, int> m;
    EXPECT_EQ(m.size(), 0u);
    EXPECT_TRUE(m.empty());
}

TEST_F(McustlTestFixture, MapInsert) {
    mcustl::map<int, int> m;
    EXPECT_TRUE(m.insert(1, 100));
    EXPECT_TRUE(m.insert(2, 200));
    EXPECT_TRUE(m.insert(3, 300));

    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(1), 100);
    EXPECT_EQ(m.at(2), 200);
    EXPECT_EQ(m.at(3), 300);
}

TEST_F(McustlTestFixture, MapInsertDuplicate) {
    mcustl::map<int, int> m;
    EXPECT_TRUE(m.insert(1, 100));
    EXPECT_FALSE(m.insert(1, 200));  // Duplicate key
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at(1), 100);  // Original value preserved
}

TEST_F(McustlTestFixture, MapInsertUnordered) {
    mcustl::map<int, int> m;
    m.insert(30, 3);
    m.insert(10, 1);
    m.insert(20, 2);

    // Keys should be sorted
    EXPECT_EQ(m.at(10), 1);
    EXPECT_EQ(m.at(20), 2);
    EXPECT_EQ(m.at(30), 3);
}

// ==================== Access ====================

TEST_F(McustlTestFixture, MapOperatorBracket) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);

    EXPECT_EQ(m[1], 10);
    EXPECT_EQ(m[2], 20);

    // operator[] on non-existent key should insert default
    m[3] = 30;
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m[3], 30);
}

TEST_F(McustlTestFixture, MapOperatorBracketModify) {
    mcustl::map<int, int> m;
    m.insert(1, 100);

    m[1] = 999;
    EXPECT_EQ(m[1], 999);
    EXPECT_EQ(m.size(), 1u);
}

TEST_F(McustlTestFixture, MapAt) {
    mcustl::map<int, int> m;
    m.insert(5, 50);
    m.insert(10, 100);

    EXPECT_EQ(m.at(5), 50);
    EXPECT_EQ(m.at(10), 100);
}

TEST_F(McustlTestFixture, MapFind) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    EXPECT_TRUE(m.find(2) != m.end());
    EXPECT_TRUE(m.find(99) == m.end());
}

TEST_F(McustlTestFixture, MapContains) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);

    EXPECT_TRUE(m.contains(1));
    EXPECT_TRUE(m.contains(2));
    EXPECT_FALSE(m.contains(3));
}

// ==================== Erase ====================

TEST_F(McustlTestFixture, MapErase) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    EXPECT_TRUE(m.erase(2));
    EXPECT_EQ(m.size(), 2u);
    EXPECT_FALSE(m.contains(2));
    EXPECT_TRUE(m.contains(1));
    EXPECT_TRUE(m.contains(3));
}

TEST_F(McustlTestFixture, MapEraseFirst) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    EXPECT_TRUE(m.erase(1));
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(2), 20);
    EXPECT_EQ(m.at(3), 30);
}

TEST_F(McustlTestFixture, MapEraseLast) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    EXPECT_TRUE(m.erase(3));
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(1), 10);
    EXPECT_EQ(m.at(2), 20);
}

TEST_F(McustlTestFixture, MapEraseNonExistent) {
    mcustl::map<int, int> m;
    m.insert(1, 10);

    EXPECT_FALSE(m.erase(99));
    EXPECT_EQ(m.size(), 1u);
}

// ==================== Capacity ====================

TEST_F(McustlTestFixture, MapSizeEmpty) {
    mcustl::map<int, int> m;
    EXPECT_EQ(m.size(), 0u);
    EXPECT_TRUE(m.empty());

    m.insert(1, 10);
    EXPECT_EQ(m.size(), 1u);
    EXPECT_FALSE(m.empty());
}

TEST_F(McustlTestFixture, MapClear) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    m.clear();
    EXPECT_EQ(m.size(), 0u);
    EXPECT_TRUE(m.empty());
    EXPECT_FALSE(m.contains(1));
}

// ==================== Sorted Order ====================

TEST_F(McustlTestFixture, MapSortedOrder) {
    mcustl::map<int, int> m;
    m.insert(50, 5);
    m.insert(10, 1);
    m.insert(30, 3);
    m.insert(20, 2);
    m.insert(40, 4);

    // Iterate and verify sorted order
    int expected_keys[] = {10, 20, 30, 40, 50};
    int expected_vals[] = {1, 2, 3, 4, 5};
    int idx = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        EXPECT_EQ(it->first, expected_keys[idx]);
        EXPECT_EQ(it->second, expected_vals[idx]);
        idx++;
    }
    EXPECT_EQ(idx, 5);
}

TEST_F(McustlTestFixture, MapSortedAfterErase) {
    mcustl::map<int, int> m;
    for (int i = 1; i <= 5; i++) m.insert(i * 10, i);

    m.erase(30);

    int expected[] = {10, 20, 40, 50};
    int idx = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        EXPECT_EQ(it->first, expected[idx++]);
    }
    EXPECT_EQ(idx, 4);
}

// ==================== Copy/Assignment ====================

TEST_F(McustlTestFixture, MapCopy) {
    mcustl::map<int, int> original;
    original.insert(1, 10);
    original.insert(2, 20);
    original.insert(3, 30);

    mcustl::map<int, int> copy = original;
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_EQ(copy.at(1), 10);
    EXPECT_EQ(copy.at(2), 20);
    EXPECT_EQ(copy.at(3), 30);

    // Modify original, copy independent
    original.erase(1);
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_TRUE(copy.contains(1));
}

TEST_F(McustlTestFixture, MapAssignment) {
    mcustl::map<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);

    mcustl::map<int, int> b;
    b.insert(10, 100);

    b = a;
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b.at(1), 10);
    EXPECT_EQ(b.at(2), 20);
    EXPECT_FALSE(b.contains(10));
}

// ==================== String Keys ====================

TEST_F(McustlTestFixture, MapStringKeys) {
    mcustl::map<mcustl::string, int> m;
    m.insert(mcustl::string("banana"), 2);
    m.insert(mcustl::string("apple"), 1);
    m.insert(mcustl::string("cherry"), 3);

    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(mcustl::string("apple")), 1);
    EXPECT_EQ(m.at(mcustl::string("banana")), 2);
    EXPECT_EQ(m.at(mcustl::string("cherry")), 3);
}

// ==================== Edge Cases ====================

TEST_F(McustlTestFixture, MapSingleElement) {
    mcustl::map<int, int> m;
    m.insert(42, 100);

    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at(42), 100);
    EXPECT_TRUE(m.contains(42));

    m.erase(42);
    EXPECT_TRUE(m.empty());
}

TEST_F(McustlTestFixture, MapEraseAll) {
    mcustl::map<int, int> m;
    for (int i = 0; i < 5; i++) m.insert(i, i * 10);

    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(m.erase(i));
    }
    EXPECT_TRUE(m.empty());
}

// ==================== Stress ====================

TEST_F(McustlTestFixture, MapStressInsertErase) {
    mcustl::map<int, int> m;

    // Insert 30 elements
    for (int i = 0; i < 30; i++) {
        ASSERT_TRUE(m.insert(i, i * 10));
    }
    EXPECT_EQ(m.size(), 30u);

    // Erase odd keys
    for (int i = 1; i < 30; i += 2) {
        ASSERT_TRUE(m.erase(i));
    }
    EXPECT_EQ(m.size(), 15u);

    // Verify remaining are even
    for (int i = 0; i < 30; i += 2) {
        EXPECT_TRUE(m.contains(i));
        EXPECT_EQ(m.at(i), i * 10);
    }
}

TEST_F(McustlTestFixture, MapStressRepeatedClearInsert) {
    mcustl::map<int, int> m;
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 10; i++) {
            m.insert(round * 100 + i, i);
        }
        m.clear();
        EXPECT_TRUE(m.empty());
    }
}

// ==================== Range-based for ====================

TEST_F(McustlTestFixture, MapRangeFor) {
    mcustl::map<int, int> m;
    m.insert(30, 3);
    m.insert(10, 1);
    m.insert(20, 2);

    int key_sum = 0, val_sum = 0;
    for (auto& p : m) {
        key_sum += p.first;
        val_sum += p.second;
    }
    EXPECT_EQ(key_sum, 60);
    EXPECT_EQ(val_sum, 6);
}

TEST_F(McustlTestFixture, MapRangeForConst) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);

    const auto& cm = m;
    int sum = 0;
    for (const auto& p : cm) sum += p.second;
    EXPECT_EQ(sum, 30);
}

TEST_F(McustlTestFixture, MapRangeForSortedOrder) {
    mcustl::map<int, int> m;
    m.insert(50, 5);
    m.insert(10, 1);
    m.insert(30, 3);
    m.insert(20, 2);
    m.insert(40, 4);

    int prev_key = -1;
    for (auto& p : m) {
        EXPECT_GT(p.first, prev_key);
        prev_key = p.first;
    }
}

TEST_F(McustlTestFixture, MapRangeForEmpty) {
    mcustl::map<int, int> m;
    int count = 0;
    for (auto& p : m) { (void)p; count++; }
    EXPECT_EQ(count, 0);
}

// ==================== Standalone pair ====================

TEST_F(McustlTestFixture, PairDefaultConstruct) {
    mcustl::pair<int, int> p;
    EXPECT_EQ(p.first, 0);
    EXPECT_EQ(p.second, 0);
}

TEST_F(McustlTestFixture, PairConstruct) {
    mcustl::pair<int, int> p(10, 20);
    EXPECT_EQ(p.first, 10);
    EXPECT_EQ(p.second, 20);
}

TEST_F(McustlTestFixture, PairCopy) {
    mcustl::pair<int, int> a(1, 2);
    mcustl::pair<int, int> b = a;
    EXPECT_EQ(b.first, 1);
    EXPECT_EQ(b.second, 2);
    a.first = 99;
    EXPECT_EQ(b.first, 1);  // independent
}

TEST_F(McustlTestFixture, PairMove) {
    mcustl::pair<int, mcustl::string> a(1, mcustl::string("hello"));
    mcustl::pair<int, mcustl::string> b = static_cast<mcustl::pair<int, mcustl::string>&&>(a);
    EXPECT_EQ(b.first, 1);
    EXPECT_STREQ(b.second.c_str(), "hello");
}

TEST_F(McustlTestFixture, PairMakePair) {
    auto p = mcustl::make_pair(42, 100);
    EXPECT_EQ(p.first, 42);
    EXPECT_EQ(p.second, 100);
}

TEST_F(McustlTestFixture, PairEquality) {
    mcustl::pair<int, int> a(1, 2);
    mcustl::pair<int, int> b(1, 2);
    mcustl::pair<int, int> c(1, 3);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
}

TEST_F(McustlTestFixture, PairLessThan) {
    mcustl::pair<int, int> a(1, 2);
    mcustl::pair<int, int> b(2, 1);
    mcustl::pair<int, int> c(1, 3);
    EXPECT_TRUE(a < b);   // first differs
    EXPECT_TRUE(a < c);   // first equal, second differs
    EXPECT_FALSE(b < a);
}

// ==================== find() returns iterator ====================

TEST_F(McustlTestFixture, MapFindReturnsIterator) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    auto it = m.find(2);
    EXPECT_TRUE(it != m.end());
    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(it->second, 20);
}

TEST_F(McustlTestFixture, MapFindNotFound) {
    mcustl::map<int, int> m;
    m.insert(1, 10);

    auto it = m.find(99);
    EXPECT_TRUE(it == m.end());
}

TEST_F(McustlTestFixture, MapFindModifyValue) {
    mcustl::map<int, int> m;
    m.insert(5, 50);

    auto it = m.find(5);
    it->second = 999;
    EXPECT_EQ(m.at(5), 999);
}

TEST_F(McustlTestFixture, MapFindConst) {
    mcustl::map<int, int> m;
    m.insert(1, 10);

    const auto& cm = m;
    auto it = cm.find(1);
    EXPECT_TRUE(it != cm.end());
    EXPECT_EQ(it->second, 10);
}

TEST_F(McustlTestFixture, MapFindEmpty) {
    mcustl::map<int, int> m;
    EXPECT_TRUE(m.find(1) == m.end());
}

// ==================== erase(iterator) ====================

TEST_F(McustlTestFixture, MapEraseByIterator) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    auto it = m.find(2);
    auto next = m.erase(it);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_FALSE(m.contains(2));
    EXPECT_TRUE(next != m.end());
    EXPECT_EQ(next->first, 3);
}

TEST_F(McustlTestFixture, MapEraseByIteratorBegin) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    auto next = m.erase(m.begin());
    EXPECT_EQ(m.size(), 2u);
    EXPECT_FALSE(m.contains(1));
    EXPECT_EQ(next->first, 2);
}

TEST_F(McustlTestFixture, MapEraseByIteratorLast) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    auto it = m.find(3);
    auto next = m.erase(it);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_TRUE(next == m.end());
}

TEST_F(McustlTestFixture, MapEraseByIteratorOnly) {
    mcustl::map<int, int> m;
    m.insert(42, 100);

    auto next = m.erase(m.begin());
    EXPECT_TRUE(m.empty());
    EXPECT_TRUE(next == m.end());
}

TEST_F(McustlTestFixture, MapEraseByIteratorEnd) {
    mcustl::map<int, int> m;
    m.insert(1, 10);

    auto next = m.erase(m.end());
    EXPECT_EQ(m.size(), 1u);  // no-op
    EXPECT_TRUE(next == m.end());
}

TEST_F(McustlTestFixture, MapFindErasePattern) {
    mcustl::map<int, int> m;
    for (int i = 0; i < 10; i++) m.insert(i, i * 10);

    // Idiomatic find-then-erase
    auto it = m.find(5);
    EXPECT_TRUE(it != m.end());
    m.erase(it);
    EXPECT_EQ(m.size(), 9u);
    EXPECT_FALSE(m.contains(5));
}

TEST_F(McustlTestFixture, MapEraseByIteratorAll) {
    mcustl::map<int, int> m;
    for (int i = 0; i < 5; i++) m.insert(i, i);

    auto it = m.begin();
    while (it != m.end()) {
        it = m.erase(it);
    }
    EXPECT_TRUE(m.empty());
}

// ==================== lower_bound / upper_bound ====================

TEST_F(McustlTestFixture, MapLowerBoundExact) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);
    m.insert(30, 3);

    auto it = m.lower_bound(20);
    EXPECT_TRUE(it != m.end());
    EXPECT_EQ(it->first, 20);
}

TEST_F(McustlTestFixture, MapLowerBoundBetween) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);
    m.insert(30, 3);

    auto it = m.lower_bound(15);
    EXPECT_TRUE(it != m.end());
    EXPECT_EQ(it->first, 20);
}

TEST_F(McustlTestFixture, MapLowerBoundBeforeAll) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);

    auto it = m.lower_bound(5);
    EXPECT_TRUE(it != m.end());
    EXPECT_EQ(it->first, 10);
}

TEST_F(McustlTestFixture, MapLowerBoundAfterAll) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);

    auto it = m.lower_bound(25);
    EXPECT_TRUE(it == m.end());
}

TEST_F(McustlTestFixture, MapUpperBoundExact) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);
    m.insert(30, 3);

    auto it = m.upper_bound(20);
    EXPECT_TRUE(it != m.end());
    EXPECT_EQ(it->first, 30);  // strictly greater
}

TEST_F(McustlTestFixture, MapUpperBoundBetween) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);
    m.insert(30, 3);

    auto it = m.upper_bound(15);
    EXPECT_TRUE(it != m.end());
    EXPECT_EQ(it->first, 20);
}

TEST_F(McustlTestFixture, MapUpperBoundAfterAll) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);

    auto it = m.upper_bound(20);
    EXPECT_TRUE(it == m.end());
}

TEST_F(McustlTestFixture, MapLowerUpperBoundRange) {
    mcustl::map<int, int> m;
    for (int i = 1; i <= 10; i++) m.insert(i * 10, i);

    // Range [30, 70)
    auto lo = m.lower_bound(30);
    auto hi = m.upper_bound(60);

    int count = 0;
    for (auto it = lo; it != hi; ++it) count++;
    EXPECT_EQ(count, 4);  // 30, 40, 50, 60
}

TEST_F(McustlTestFixture, MapLowerBoundConst) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);

    const auto& cm = m;
    auto it = cm.lower_bound(15);
    EXPECT_TRUE(it != cm.end());
    EXPECT_EQ(it->first, 20);
}

// ==================== count ====================

TEST_F(McustlTestFixture, MapCountExisting) {
    mcustl::map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);

    EXPECT_EQ(m.count(1), 1u);
    EXPECT_EQ(m.count(2), 1u);
}

TEST_F(McustlTestFixture, MapCountNonExisting) {
    mcustl::map<int, int> m;
    m.insert(1, 10);

    EXPECT_EQ(m.count(99), 0u);
    EXPECT_EQ(m.count(0), 0u);
}

// ==================== swap ====================

TEST_F(McustlTestFixture, MapSwapBasic) {
    mcustl::map<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);

    mcustl::map<int, int> b;
    b.insert(100, 1000);

    a.swap(b);

    EXPECT_EQ(a.size(), 1u);
    EXPECT_EQ(a.at(100), 1000);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b.at(1), 10);
    EXPECT_EQ(b.at(2), 20);
}

TEST_F(McustlTestFixture, MapSwapWithEmpty) {
    mcustl::map<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);

    mcustl::map<int, int> b;

    a.swap(b);

    EXPECT_TRUE(a.empty());
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b.at(1), 10);
}

TEST_F(McustlTestFixture, MapSwapBothEmpty) {
    mcustl::map<int, int> a;
    mcustl::map<int, int> b;

    a.swap(b);

    EXPECT_TRUE(a.empty());
    EXPECT_TRUE(b.empty());
}

TEST_F(McustlTestFixture, MapSwapIterationAfter) {
    mcustl::map<int, int> a;
    a.insert(3, 30);
    a.insert(1, 10);
    a.insert(2, 20);

    mcustl::map<int, int> b;
    b.insert(50, 5);
    b.insert(40, 4);

    a.swap(b);

    // Verify sorted iteration works after swap
    int prev = -1;
    for (auto& p : a) {
        EXPECT_GT(p.first, prev);
        prev = p.first;
    }
    prev = -1;
    for (auto& p : b) {
        EXPECT_GT(p.first, prev);
        prev = p.first;
    }
}

// ==================== Iterator bidirectional ====================

TEST_F(McustlTestFixture, MapIteratorDecrement) {
    mcustl::map<int, int> m;
    m.insert(10, 1);
    m.insert(20, 2);
    m.insert(30, 3);

    auto it = m.end();
    --it;
    EXPECT_EQ(it->first, 30);
    --it;
    EXPECT_EQ(it->first, 20);
    --it;
    EXPECT_EQ(it->first, 10);
    EXPECT_TRUE(it == m.begin());
}

TEST_F(McustlTestFixture, MapIteratorReverseTraversal) {
    mcustl::map<int, int> m;
    for (int i = 1; i <= 5; i++) m.insert(i * 10, i);

    int expected[] = {50, 40, 30, 20, 10};
    auto it = m.end();
    for (int i = 0; i < 5; i++) {
        --it;
        EXPECT_EQ(it->first, expected[i]);
    }
}

// ==================== Stress: sequential keys (worst case for naive BST) ====================

TEST_F(McustlTestFixture, MapStressSequentialKeys) {
    mcustl::map<int, int> m;

    // Insert 50 keys in ascending order — RB tree keeps O(log n) depth
    for (int i = 0; i < 50; i++) {
        ASSERT_TRUE(m.insert(i, i * 10));
    }
    EXPECT_EQ(m.size(), 50u);

    // Verify all present and sorted
    int prev = -1;
    int count = 0;
    for (auto& p : m) {
        EXPECT_GT(p.first, prev);
        prev = p.first;
        count++;
    }
    EXPECT_EQ(count, 50);

    // Erase every third
    for (int i = 0; i < 50; i += 3) {
        EXPECT_TRUE(m.erase(i));
    }

    // Verify remaining sorted
    prev = -1;
    for (auto& p : m) {
        EXPECT_GT(p.first, prev);
        prev = p.first;
    }
}

TEST_F(McustlTestFixture, MapStressDescendingKeys) {
    mcustl::map<int, int> m;

    for (int i = 49; i >= 0; i--) {
        ASSERT_TRUE(m.insert(i, i));
    }
    EXPECT_EQ(m.size(), 50u);

    // Should still iterate in ascending order
    int prev = -1;
    for (auto& p : m) {
        EXPECT_GT(p.first, prev);
        prev = p.first;
    }
}

// ==================== Large map ====================

TEST_F(McustlTestFixture, MapLargeInsertEraseFind) {
    mcustl::map<int, int> m;

    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(m.insert(i, i * 7));
    }
    EXPECT_EQ(m.size(), 100u);

    for (int i = 0; i < 100; i++) {
        auto it = m.find(i);
        ASSERT_TRUE(it != m.end());
        EXPECT_EQ(it->second, i * 7);
    }

    // Erase even keys
    for (int i = 0; i < 100; i += 2) {
        ASSERT_TRUE(m.erase(i));
    }
    EXPECT_EQ(m.size(), 50u);

    // Verify only odd remain
    for (int i = 0; i < 100; i++) {
        if (i % 2 == 0) {
            EXPECT_TRUE(m.find(i) == m.end());
        } else {
            EXPECT_TRUE(m.find(i) != m.end());
        }
    }
}

// ==================== Return from Function ====================

static mcustl::map<int, int> make_int_map(int n) {
    mcustl::map<int, int> m;
    for (int i = 0; i < n; i++) m.insert(i, i * 100);
    return m;
}

TEST_F(McustlTestFixture, MapReturnFromFunction) {
    mcustl::map<int, int> m = make_int_map(5);
    EXPECT_EQ(m.size(), 5u);
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(m.contains(i));
        EXPECT_EQ(m.at(i), i * 100);
    }
}

TEST_F(McustlTestFixture, MapReturnAndModify) {
    mcustl::map<int, int> m = make_int_map(3);
    m.insert(10, 1000);
    m.erase(1);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_FALSE(m.contains(1));
    EXPECT_TRUE(m.contains(10));
    EXPECT_EQ(m.at(10), 1000);
}

static mcustl::map<int, mcustl::string> make_string_map() {
    mcustl::map<int, mcustl::string> m;
    m.insert(1, mcustl::string("one"));
    m.insert(2, mcustl::string("two"));
    m.insert(3, mcustl::string("three"));
    return m;
}

TEST_F(McustlTestFixture, MapReturnOfStrings) {
    mcustl::map<int, mcustl::string> m = make_string_map();
    EXPECT_EQ(m.size(), 3u);
    EXPECT_STREQ(m.at(1).c_str(), "one");
    EXPECT_STREQ(m.at(2).c_str(), "two");
    EXPECT_STREQ(m.at(3).c_str(), "three");
}

TEST_F(McustlTestFixture, MapReturnSortedIteration) {
    mcustl::map<int, int> m = make_int_map(10);

    int prev = -1;
    for (auto& kv : m) {
        EXPECT_GT(kv.first, prev);
        prev = kv.first;
    }
}

static mcustl::map<int, IntegrityObject> make_integrity_map(int n) {
    mcustl::map<int, IntegrityObject> m;
    for (int i = 0; i < n; i++) m.insert(i, IntegrityObject(i * 10));
    return m;
}

TEST_F(McustlTestFixture, MapReturnIntegrity) {
    IntegrityObject::reset_counters();
    {
        mcustl::map<int, IntegrityObject> m = make_integrity_map(5);
        EXPECT_EQ(m.size(), 5u);
        for (auto& kv : m) {
            EXPECT_TRUE(kv.second.is_valid());
        }
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}

TEST_F(McustlTestFixture, MapReturnAssign) {
    mcustl::map<int, int> m;
    m.insert(999, 1);
    m = make_int_map(4);
    EXPECT_EQ(m.size(), 4u);
    EXPECT_FALSE(m.contains(999));
    EXPECT_EQ(m.at(0), 0);
    EXPECT_EQ(m.at(3), 300);
}

// Conditional return prevents NRVO — forces move constructor
static mcustl::map<int, int> make_map_conditional(bool flag) {
    mcustl::map<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);
    mcustl::map<int, int> b;
    b.insert(100, 1000);
    b.insert(200, 2000);
    b.insert(300, 3000);
    if (flag) return a;
    return b;
}

TEST_F(McustlTestFixture, MapReturnConditionalTrue) {
    mcustl::map<int, int> m = make_map_conditional(true);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(1), 10);
    EXPECT_EQ(m.at(2), 20);
    m.insert(3, 30);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(3), 30);
}

TEST_F(McustlTestFixture, MapReturnConditionalFalse) {
    mcustl::map<int, int> m = make_map_conditional(false);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(100), 1000);
    EXPECT_EQ(m.at(200), 2000);
    EXPECT_EQ(m.at(300), 3000);
    m.erase(200);
    EXPECT_EQ(m.size(), 2u);
}

TEST_F(McustlTestFixture, MapReturnConditionalIteration) {
    mcustl::map<int, int> m = make_map_conditional(false);
    int prev = -1;
    for (auto& kv : m) {
        EXPECT_GT(kv.first, prev);
        prev = kv.first;
    }
}

static mcustl::map<int, IntegrityObject> make_integrity_map_conditional(bool flag) {
    mcustl::map<int, IntegrityObject> a;
    a.insert(1, IntegrityObject(10));
    mcustl::map<int, IntegrityObject> b;
    b.insert(1, IntegrityObject(100));
    b.insert(2, IntegrityObject(200));
    if (flag) return a;
    return b;
}

TEST_F(McustlTestFixture, MapReturnConditionalIntegrity) {
    IntegrityObject::reset_counters();
    {
        mcustl::map<int, IntegrityObject> m = make_integrity_map_conditional(true);
        for (auto& kv : m) {
            EXPECT_TRUE(kv.second.is_valid());
        }
        m.insert(5, IntegrityObject(50));
        EXPECT_TRUE(m.at(5).is_valid());
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}
