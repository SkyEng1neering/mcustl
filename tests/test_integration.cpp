/**
 * @file test_integration.cpp
 * @brief Integration tests: vector, string, and smart_ptr used together
 *
 * Tests cross-module interactions and edge cases that only manifest
 * when multiple container types share the same dalloc heap.
 */

#include "mcustl_test_config.h"

// ==================== Vector of Strings ====================

TEST_F(McustlTestFixture, VectorOfStrings_Basic) {
    mcustl::vector<mcustl::string> vec;
    mcustl::string s1("Hello");
    mcustl::string s2("World");
    mcustl::string s3("Test");

    EXPECT_TRUE(vec.push_back(s1));
    EXPECT_TRUE(vec.push_back(s2));
    EXPECT_TRUE(vec.push_back(s3));

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_STREQ(vec[0].c_str(), "Hello");
    EXPECT_STREQ(vec[1].c_str(), "World");
    EXPECT_STREQ(vec[2].c_str(), "Test");
}

TEST_F(McustlTestFixture, VectorOfStrings_PopMiddle) {
    mcustl::vector<mcustl::string> vec;
    vec.push_back(mcustl::string("AAA"));
    vec.push_back(mcustl::string("BBB"));
    vec.push_back(mcustl::string("CCC"));
    vec.push_back(mcustl::string("DDD"));

    // Pop middle element - triggers defrag
    EXPECT_TRUE(vec.pop(1));

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_STREQ(vec[0].c_str(), "AAA");
    EXPECT_STREQ(vec[1].c_str(), "CCC");
    EXPECT_STREQ(vec[2].c_str(), "DDD");
}

TEST_F(McustlTestFixture, VectorOfStrings_PopFirst) {
    mcustl::vector<mcustl::string> vec;
    vec.push_back(mcustl::string("First"));
    vec.push_back(mcustl::string("Second"));
    vec.push_back(mcustl::string("Third"));

    EXPECT_TRUE(vec.pop(0));

    EXPECT_EQ(vec.size(), 2u);
    EXPECT_STREQ(vec[0].c_str(), "Second");
    EXPECT_STREQ(vec[1].c_str(), "Third");
}

TEST_F(McustlTestFixture, VectorOfStrings_PopLast) {
    mcustl::vector<mcustl::string> vec;
    vec.push_back(mcustl::string("First"));
    vec.push_back(mcustl::string("Second"));
    vec.push_back(mcustl::string("Third"));

    EXPECT_TRUE(vec.pop_back());

    EXPECT_EQ(vec.size(), 2u);
    EXPECT_STREQ(vec[0].c_str(), "First");
    EXPECT_STREQ(vec[1].c_str(), "Second");
}

TEST_F(McustlTestFixture, VectorOfStrings_Clear) {
    mcustl::vector<mcustl::string> vec;
    vec.push_back(mcustl::string("A"));
    vec.push_back(mcustl::string("BB"));
    vec.push_back(mcustl::string("CCC"));

    vec.clear();
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.empty());
}

TEST_F(McustlTestFixture, VectorOfStrings_GrowMultiple) {
    mcustl::vector<mcustl::string> vec;
    const char* words[] = {"one", "two", "three", "four", "five",
                           "six", "seven", "eight", "nine", "ten"};
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(vec.push_back(mcustl::string(words[i])));
    }

    for (int i = 0; i < 10; i++) {
        EXPECT_STREQ(vec[i].c_str(), words[i]);
    }
}

TEST_F(McustlTestFixture, VectorOfStrings_ModifyAfterPush) {
    mcustl::vector<mcustl::string> vec;
    vec.push_back(mcustl::string("Hello"));
    vec.push_back(mcustl::string("World"));

    // Modify string inside vector
    vec[0] += " Modified";
    EXPECT_STREQ(vec[0].c_str(), "Hello Modified");
    EXPECT_STREQ(vec[1].c_str(), "World");  // Unaffected
}

// ==================== SmartPtr to Vector ====================

TEST_F(McustlTestFixture, SmartPtrToVector) {
    mcustl::smart_ptr<mcustl::vector<int>> pvec;
    EXPECT_TRUE(pvec.create());
    pvec->push_back(10);
    pvec->push_back(20);
    pvec->push_back(30);

    EXPECT_EQ(pvec->size(), 3u);
    EXPECT_EQ((*pvec)[0], 10);
    EXPECT_EQ((*pvec)[1], 20);
    EXPECT_EQ((*pvec)[2], 30);
}

TEST_F(McustlTestFixture, SmartPtrToVector_Reset) {
    mcustl::smart_ptr<mcustl::vector<int>> pvec;
    EXPECT_TRUE(pvec.create());
    pvec->push_back(1);
    pvec->push_back(2);

    pvec.reset();
    EXPECT_EQ(pvec.get(), nullptr);
}

// ==================== SmartPtr to String ====================

TEST_F(McustlTestFixture, SmartPtrToString) {
    mcustl::smart_ptr<mcustl::string> pstr;
    EXPECT_TRUE(pstr.create("Hello Smart"));
    EXPECT_STREQ(pstr->c_str(), "Hello Smart");
    EXPECT_EQ(pstr->size(), 11u);
}

TEST_F(McustlTestFixture, SmartPtrToString_Modify) {
    mcustl::smart_ptr<mcustl::string> pstr;
    EXPECT_TRUE(pstr.create("Base"));
    *pstr += " Extended";
    EXPECT_STREQ(pstr->c_str(), "Base Extended");
}

TEST_F(McustlTestFixture, SmartPtrToString_MoveOwnership) {
    mcustl::smart_ptr<mcustl::string> p1;
    p1.create("Movable");

    mcustl::smart_ptr<mcustl::string> p2(static_cast<mcustl::smart_ptr<mcustl::string>&&>(p1));
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_STREQ(p2->c_str(), "Movable");
}

// ==================== Vector of SmartPtrs ====================

TEST_F(McustlTestFixture, VectorOfSmartPtrs) {
    mcustl::vector<mcustl::smart_ptr<int>> vec;

    mcustl::smart_ptr<int> p1;
    p1.allocate(1); *p1 = 100;
    vec.push_back(static_cast<mcustl::smart_ptr<int>&&>(p1));

    mcustl::smart_ptr<int> p2;
    p2.allocate(1); *p2 = 200;
    vec.push_back(static_cast<mcustl::smart_ptr<int>&&>(p2));

    mcustl::smart_ptr<int> p3;
    p3.allocate(1); *p3 = 300;
    vec.push_back(static_cast<mcustl::smart_ptr<int>&&>(p3));

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(*vec[0], 100);
    EXPECT_EQ(*vec[1], 200);
    EXPECT_EQ(*vec[2], 300);
}

TEST_F(McustlTestFixture, VectorOfSmartPtrs_PopMiddle) {
    mcustl::vector<mcustl::smart_ptr<int>> vec;

    for (int i = 0; i < 4; i++) {
        mcustl::smart_ptr<int> p;
        p.allocate(1);
        *p = i * 10;
        vec.push_back(static_cast<mcustl::smart_ptr<int>&&>(p));
    }

    EXPECT_TRUE(vec.pop(1));

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(*vec[0], 0);
    EXPECT_EQ(*vec[1], 20);
    EXPECT_EQ(*vec[2], 30);
}

TEST_F(McustlTestFixture, VectorOfSmartPtrs_Clear) {
    mcustl::vector<mcustl::smart_ptr<TrackedObject>> vec;
    TrackedObject::resetCounters();

    for (int i = 0; i < 3; i++) {
        mcustl::smart_ptr<TrackedObject> p;
        p.create(i);
        vec.push_back(static_cast<mcustl::smart_ptr<TrackedObject>&&>(p));
    }

    vec.clear();
    EXPECT_EQ(vec.size(), 0u);
}

// ==================== Nested Vectors ====================

TEST_F(McustlTestFixture, NestedVectors) {
    mcustl::vector<mcustl::vector<int>> outer;

    mcustl::vector<int> inner1;
    inner1.push_back(1); inner1.push_back(2);

    mcustl::vector<int> inner2;
    inner2.push_back(10); inner2.push_back(20); inner2.push_back(30);

    outer.push_back(inner1);
    outer.push_back(inner2);

    EXPECT_EQ(outer.size(), 2u);
    EXPECT_EQ(outer[0].size(), 2u);
    EXPECT_EQ(outer[0][0], 1);
    EXPECT_EQ(outer[0][1], 2);
    EXPECT_EQ(outer[1].size(), 3u);
    EXPECT_EQ(outer[1][0], 10);
    EXPECT_EQ(outer[1][1], 20);
    EXPECT_EQ(outer[1][2], 30);
}

TEST_F(McustlTestFixture, NestedVectors_PopOuter) {
    mcustl::vector<mcustl::vector<int>> outer;

    mcustl::vector<int> v1;
    v1.push_back(1);
    mcustl::vector<int> v2;
    v2.push_back(2);
    mcustl::vector<int> v3;
    v3.push_back(3);

    outer.push_back(v1);
    outer.push_back(v2);
    outer.push_back(v3);

    outer.pop(0);  // Remove first, triggers defrag

    EXPECT_EQ(outer.size(), 2u);
    EXPECT_EQ(outer[0][0], 2);
    EXPECT_EQ(outer[1][0], 3);
}

// ==================== Deep Nesting: SmartPtr -> Vector -> String ====================

TEST_F(McustlTestFixture, SmartPtrToVectorOfStrings) {
    mcustl::smart_ptr<mcustl::vector<mcustl::string>> pvec;
    EXPECT_TRUE(pvec.create());

    pvec->push_back(mcustl::string("Alpha"));
    pvec->push_back(mcustl::string("Beta"));
    pvec->push_back(mcustl::string("Gamma"));

    EXPECT_EQ(pvec->size(), 3u);
    EXPECT_STREQ((*pvec)[0].c_str(), "Alpha");
    EXPECT_STREQ((*pvec)[1].c_str(), "Beta");
    EXPECT_STREQ((*pvec)[2].c_str(), "Gamma");
}

TEST_F(McustlTestFixture, SmartPtrToVectorOfStrings_Modify) {
    mcustl::smart_ptr<mcustl::vector<mcustl::string>> pvec;
    pvec.create();

    pvec->push_back(mcustl::string("Hello"));
    (*pvec)[0] += " World";

    EXPECT_STREQ((*pvec)[0].c_str(), "Hello World");
}

// ==================== Mixed Containers Sharing Heap ====================

TEST_F(McustlTestFixture, MixedContainersCoexist) {
    mcustl::vector<int> vec;
    mcustl::string str("TestString");
    mcustl::smart_ptr<int> ptr;

    vec.push_back(42);
    ptr.allocate(1);
    *ptr = 99;

    EXPECT_EQ(vec[0], 42);
    EXPECT_STREQ(str.c_str(), "TestString");
    EXPECT_EQ(*ptr, 99);
}

TEST_F(McustlTestFixture, MixedContainers_FreeMiddle) {
    mcustl::smart_ptr<int> p1;
    p1.allocate(1); *p1 = 10;

    mcustl::string str("Middle");

    mcustl::vector<int> vec;
    vec.push_back(20);

    // Free string (middle allocation) - triggers defrag
    str.clear();
    str.shrink_to_fit();

    // Others should survive
    EXPECT_EQ(*p1, 10);
    EXPECT_EQ(vec[0], 20);
}

TEST_F(McustlTestFixture, MixedContainers_InterleavedAlloc) {
    mcustl::vector<int> v1;
    v1.push_back(1);

    mcustl::string s1("A");

    mcustl::vector<int> v2;
    v2.push_back(2);

    mcustl::string s2("B");

    mcustl::smart_ptr<int> p1;
    p1.allocate(1); *p1 = 3;

    EXPECT_EQ(v1[0], 1);
    EXPECT_STREQ(s1.c_str(), "A");
    EXPECT_EQ(v2[0], 2);
    EXPECT_STREQ(s2.c_str(), "B");
    EXPECT_EQ(*p1, 3);
}

// ==================== Vector of Strings with SmartPtr ====================

TEST_F(McustlTestFixture, SmartPtrAndVectorOfStrings_Coexist) {
    mcustl::smart_ptr<int> guard;
    guard.allocate(1);
    *guard = 0xDEAD;

    mcustl::vector<mcustl::string> vec;
    vec.push_back(mcustl::string("First"));
    vec.push_back(mcustl::string("Second"));

    // Guard should survive vector operations
    EXPECT_EQ(*guard, 0xDEAD);
    EXPECT_STREQ(vec[0].c_str(), "First");
    EXPECT_STREQ(vec[1].c_str(), "Second");

    // Pop from vector, guard survives defrag
    vec.pop(0);
    EXPECT_EQ(*guard, 0xDEAD);
    EXPECT_STREQ(vec[0].c_str(), "Second");
}

// ==================== Edge Cases ====================

TEST_F(McustlTestFixture, EmptyVectorOfStrings) {
    mcustl::vector<mcustl::string> vec;
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.empty());

    // Push then immediately pop
    vec.push_back(mcustl::string("temp"));
    vec.pop_back();
    EXPECT_TRUE(vec.empty());
}

TEST_F(McustlTestFixture, SingleElementVectorOfSmartPtrs) {
    mcustl::vector<mcustl::smart_ptr<int>> vec;

    mcustl::smart_ptr<int> p;
    p.allocate(1); *p = 42;
    vec.push_back(static_cast<mcustl::smart_ptr<int>&&>(p));

    EXPECT_EQ(vec.size(), 1u);
    EXPECT_EQ(*vec[0], 42);

    vec.pop_back();
    EXPECT_TRUE(vec.empty());
}

TEST_F(McustlTestFixture, VectorOfStrings_ShrinkToFit) {
    mcustl::vector<mcustl::string> vec;
    vec.reserve(10);
    vec.push_back(mcustl::string("only"));

    EXPECT_GT(vec.capacity(), vec.size());
    EXPECT_TRUE(vec.shrink_to_fit());
    EXPECT_EQ(vec.capacity(), vec.size());
    EXPECT_STREQ(vec[0].c_str(), "only");
}

TEST_F(McustlTestFixture, RepeatedCreateDestroy) {
    for (int round = 0; round < 5; round++) {
        mcustl::vector<mcustl::string> vec;
        for (int i = 0; i < 5; i++) {
            mcustl::string s("item");
            s += ('0' + (char)i);
            vec.push_back(s);
        }
        EXPECT_EQ(vec.size(), 5u);
        // vec destroyed at end of scope, heap defragmented
    }
    // Heap should be clean now
    EXPECT_EQ(mcustl_get_default_heap()->alloc_info.allocations_num, 0u);
    EXPECT_EQ(mcustl_get_default_heap()->offset, 0u);
}

// ==================== Integrity Under Defrag ====================

TEST_F(McustlTestFixture, IntegrityObjectsInVectorDefrag) {
    IntegrityObject::reset_counters();
    {
        mcustl::vector<IntegrityObject> vec;
        for (int i = 0; i < 8; i++) {
            vec.push_back(IntegrityObject(i));
        }

        // Pop from various positions
        vec.pop(3);
        vec.pop(0);
        vec.pop(vec.size() - 1);

        for (uint32_t i = 0; i < vec.size(); i++) {
            EXPECT_TRUE(vec[i].is_valid()) << "Corruption at index " << i;
        }
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}

TEST_F(McustlTestFixture, IntegrityObjectsInSmartPtr) {
    IntegrityObject::reset_counters();
    {
        mcustl::smart_ptr<IntegrityObject> p;
        p.create(42);
        EXPECT_TRUE(p->is_valid());
        EXPECT_EQ(p->value, 42);
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}

// ==================== Stress: All Types Interleaved ====================

TEST_F(McustlTestFixture, StressMixedContainers) {
    for (int round = 0; round < 3; round++) {
        mcustl::vector<int> vi;
        mcustl::string str;
        mcustl::smart_ptr<int> sp;
        sp.allocate(1);

        for (int i = 0; i < 10; i++) {
            vi.push_back(i);
            str.push_back('A' + (char)(i % 26));
            *sp = i;
        }

        EXPECT_EQ(vi.size(), 10u);
        EXPECT_EQ(str.size(), 10u);
        EXPECT_EQ(*sp, 9);

        // Free in reverse order
        sp.reset();
        str.clear();
        vi.clear();
    }
}

TEST_F(McustlTestFixture, StressVectorOfSmartPtrsToStrings) {
    mcustl::vector<mcustl::smart_ptr<mcustl::string>> vec;

    for (int i = 0; i < 5; i++) {
        mcustl::smart_ptr<mcustl::string> p;
        const char* texts[] = {"alpha", "beta", "gamma", "delta", "epsilon"};
        p.create(texts[i]);
        vec.push_back(static_cast<mcustl::smart_ptr<mcustl::string>&&>(p));
    }

    EXPECT_EQ(vec.size(), 5u);
    EXPECT_STREQ(vec[0]->c_str(), "alpha");
    EXPECT_STREQ(vec[4]->c_str(), "epsilon");

    // Pop middle elements
    vec.pop(2);
    vec.pop(1);

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_STREQ(vec[0]->c_str(), "alpha");
    EXPECT_STREQ(vec[1]->c_str(), "delta");
    EXPECT_STREQ(vec[2]->c_str(), "epsilon");
}

// ==================== List of Strings ====================

TEST_F(McustlTestFixture, ListOfStrings) {
    mcustl::list<mcustl::string> lst;
    lst.push_back(mcustl::string("Hello"));
    lst.push_back(mcustl::string("World"));
    lst.push_back(mcustl::string("Test"));

    EXPECT_EQ(lst.size(), 3u);
    EXPECT_STREQ(lst.front().c_str(), "Hello");
    EXPECT_STREQ(lst.back().c_str(), "Test");

    lst.pop_front();
    EXPECT_STREQ(lst.front().c_str(), "World");
}

TEST_F(McustlTestFixture, VectorOfLists) {
    mcustl::vector<mcustl::list<int>> vec;

    mcustl::list<int> l1;
    l1.push_back(1); l1.push_back(2);

    mcustl::list<int> l2;
    l2.push_back(10); l2.push_back(20); l2.push_back(30);

    vec.push_back(l1);
    vec.push_back(l2);

    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0].size(), 2u);
    EXPECT_EQ(vec[0].front(), 1);
    EXPECT_EQ(vec[1].size(), 3u);
    EXPECT_EQ(vec[1].back(), 30);
}

TEST_F(McustlTestFixture, MapOfStrings) {
    mcustl::map<int, mcustl::string> m;
    m.insert(1, mcustl::string("one"));
    m.insert(2, mcustl::string("two"));
    m.insert(3, mcustl::string("three"));

    EXPECT_EQ(m.size(), 3u);
    EXPECT_STREQ(m.at(1).c_str(), "one");
    EXPECT_STREQ(m.at(2).c_str(), "two");
    EXPECT_STREQ(m.at(3).c_str(), "three");
}

TEST_F(McustlTestFixture, SmartPtrToList) {
    mcustl::smart_ptr<mcustl::list<int>> plst;
    EXPECT_TRUE(plst.create());
    plst->push_back(10);
    plst->push_back(20);
    plst->push_back(30);

    EXPECT_EQ(plst->size(), 3u);
    EXPECT_EQ(plst->front(), 10);
    EXPECT_EQ(plst->back(), 30);

    plst.reset();
    EXPECT_EQ(plst.get(), nullptr);
}

TEST_F(McustlTestFixture, SmartPtrToMap) {
    mcustl::smart_ptr<mcustl::map<int, int>> pmap;
    EXPECT_TRUE(pmap.create());
    pmap->insert(1, 100);
    pmap->insert(2, 200);

    EXPECT_EQ(pmap->size(), 2u);
    EXPECT_EQ(pmap->at(1), 100);
    EXPECT_EQ(pmap->at(2), 200);
}

TEST_F(McustlTestFixture, MapWithVectorValues) {
    mcustl::map<int, mcustl::vector<int>> m;

    mcustl::vector<int> v1;
    v1.push_back(1); v1.push_back(2);
    m.insert(10, v1);

    mcustl::vector<int> v2;
    v2.push_back(10); v2.push_back(20); v2.push_back(30);
    m.insert(20, v2);

    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(10).size(), 2u);
    EXPECT_EQ(m.at(10)[0], 1);
    EXPECT_EQ(m.at(20).size(), 3u);
    EXPECT_EQ(m.at(20)[2], 30);
}

TEST_F(McustlTestFixture, ListOfMaps) {
    mcustl::list<mcustl::map<int, int>> lst;

    mcustl::map<int, int> m1;
    m1.insert(1, 10);
    lst.push_back(m1);

    mcustl::map<int, int> m2;
    m2.insert(2, 20);
    m2.insert(3, 30);
    lst.push_back(m2);

    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(lst.front().at(1), 10);
    EXPECT_EQ(lst.back().at(3), 30);
}

TEST_F(McustlTestFixture, StressMixedAllContainers) {
    for (int round = 0; round < 3; round++) {
        mcustl::vector<int> vec;
        mcustl::string str;
        mcustl::list<int> lst;
        mcustl::map<int, int> m;
        mcustl::smart_ptr<int> sp;
        sp.allocate(1);

        for (int i = 0; i < 5; i++) {
            vec.push_back(i);
            str.push_back('A' + (char)i);
            lst.push_back(i * 10);
            m.insert(i, i * 100);
            *sp = i;
        }

        EXPECT_EQ(vec.size(), 5u);
        EXPECT_EQ(str.size(), 5u);
        EXPECT_EQ(lst.size(), 5u);
        EXPECT_EQ(m.size(), 5u);
        EXPECT_EQ(*sp, 4);
    }
}

// ==================== Copy Semantics Across Types ====================

TEST_F(McustlTestFixture, CopyVectorOfStrings) {
    mcustl::vector<mcustl::string> original;
    original.push_back(mcustl::string("Hello"));
    original.push_back(mcustl::string("World"));

    mcustl::vector<mcustl::string> copy = original;

    EXPECT_EQ(copy.size(), 2u);
    EXPECT_STREQ(copy[0].c_str(), "Hello");
    EXPECT_STREQ(copy[1].c_str(), "World");

    // Modify original, copy should be independent
    original[0] += " Modified";
    EXPECT_STREQ(copy[0].c_str(), "Hello");
}

TEST_F(McustlTestFixture, AssignVectorOfStrings) {
    mcustl::vector<mcustl::string> v1;
    v1.push_back(mcustl::string("A"));
    v1.push_back(mcustl::string("B"));

    mcustl::vector<mcustl::string> v2;
    v2.push_back(mcustl::string("X"));

    v2 = v1;

    EXPECT_EQ(v2.size(), 2u);
    EXPECT_STREQ(v2[0].c_str(), "A");
    EXPECT_STREQ(v2[1].c_str(), "B");
}

// ==================== Heap Cleanliness ====================

TEST_F(McustlTestFixture, HeapCleanAfterAllDestroyed) {
    {
        mcustl::vector<mcustl::string> vec;
        vec.push_back(mcustl::string("Test1"));
        vec.push_back(mcustl::string("Test2"));

        mcustl::smart_ptr<int> p;
        p.allocate(5);
    }
    // All containers destroyed, heap should be fully free
    EXPECT_EQ(mcustl_get_default_heap()->alloc_info.allocations_num, 0u);
    EXPECT_EQ(mcustl_get_default_heap()->offset, 0u);
}
