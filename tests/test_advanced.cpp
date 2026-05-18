/**
 * @file test_advanced.cpp
 * @brief Advanced tests: deep nesting, defrag correctness, multithreaded access
 */

#include "mcustl_test_config.h"
#include <thread>
#include <atomic>

// ==================== Map of SmartPtrs ====================

TEST_F(McustlTestFixture, MapOfSmartPtrs) {
    mcustl::map<int, mcustl::smart_ptr<mcustl::string>> m;

    mcustl::smart_ptr<mcustl::string> p1;
    p1.create("alpha");
    m.insert(1, static_cast<mcustl::smart_ptr<mcustl::string>&&>(p1));

    mcustl::smart_ptr<mcustl::string> p2;
    p2.create("beta");
    m.insert(2, static_cast<mcustl::smart_ptr<mcustl::string>&&>(p2));

    mcustl::smart_ptr<mcustl::string> p3;
    p3.create("gamma");
    m.insert(3, static_cast<mcustl::smart_ptr<mcustl::string>&&>(p3));

    EXPECT_EQ(m.size(), 3u);
    EXPECT_STREQ(m.at(1)->c_str(), "alpha");
    EXPECT_STREQ(m.at(2)->c_str(), "beta");
    EXPECT_STREQ(m.at(3)->c_str(), "gamma");
}

TEST_F(McustlTestFixture, MapOfSmartPtrs_Erase) {
    mcustl::map<int, mcustl::smart_ptr<int>> m;

    for (int i = 0; i < 5; i++) {
        mcustl::smart_ptr<int> p;
        p.allocate(1);
        *p = i * 100;
        m.insert(i, static_cast<mcustl::smart_ptr<int>&&>(p));
    }

    EXPECT_EQ(m.size(), 5u);
    EXPECT_EQ(*m.at(2), 200);

    // Erase middle - smart_ptr should be properly destroyed
    m.erase(2);
    EXPECT_EQ(m.size(), 4u);
    EXPECT_FALSE(m.contains(2));
    EXPECT_EQ(*m.at(0), 0);
    EXPECT_EQ(*m.at(4), 400);
}

// ==================== Map of Maps ====================

TEST_F(McustlTestFixture, MapOfMaps) {
    mcustl::map<int, mcustl::map<int, int>> outer;

    mcustl::map<int, int> inner1;
    inner1.insert(1, 10);
    inner1.insert(2, 20);
    outer.insert(100, inner1);

    mcustl::map<int, int> inner2;
    inner2.insert(3, 30);
    inner2.insert(4, 40);
    inner2.insert(5, 50);
    outer.insert(200, inner2);

    EXPECT_EQ(outer.size(), 2u);
    EXPECT_EQ(outer.at(100).size(), 2u);
    EXPECT_EQ(outer.at(100).at(1), 10);
    EXPECT_EQ(outer.at(100).at(2), 20);
    EXPECT_EQ(outer.at(200).size(), 3u);
    EXPECT_EQ(outer.at(200).at(5), 50);
}

TEST_F(McustlTestFixture, MapOfMaps_Erase) {
    mcustl::map<int, mcustl::map<int, int>> outer;

    for (int i = 0; i < 3; i++) {
        mcustl::map<int, int> inner;
        for (int j = 0; j < 3; j++) {
            inner.insert(j, i * 10 + j);
        }
        outer.insert(i, inner);
    }

    outer.erase(1);  // Free middle inner map
    EXPECT_EQ(outer.size(), 2u);
    EXPECT_EQ(outer.at(0).at(0), 0);
    EXPECT_EQ(outer.at(2).at(2), 22);
}

// ==================== Vector of Map of SmartPtrs ====================

TEST_F(McustlTestFixture, VectorOfMapOfSmartPtrs) {
    mcustl::vector<mcustl::map<int, mcustl::smart_ptr<int>>> vec;

    mcustl::map<int, mcustl::smart_ptr<int>> m1;
    {
        mcustl::smart_ptr<int> p; p.allocate(1); *p = 100;
        m1.insert(1, static_cast<mcustl::smart_ptr<int>&&>(p));
    }
    {
        mcustl::smart_ptr<int> p; p.allocate(1); *p = 200;
        m1.insert(2, static_cast<mcustl::smart_ptr<int>&&>(p));
    }
    vec.push_back(static_cast<mcustl::map<int, mcustl::smart_ptr<int>>&&>(m1));

    mcustl::map<int, mcustl::smart_ptr<int>> m2;
    {
        mcustl::smart_ptr<int> p; p.allocate(1); *p = 300;
        m2.insert(10, static_cast<mcustl::smart_ptr<int>&&>(p));
    }
    vec.push_back(static_cast<mcustl::map<int, mcustl::smart_ptr<int>>&&>(m2));

    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(*vec[0].at(1), 100);
    EXPECT_EQ(*vec[0].at(2), 200);
    EXPECT_EQ(*vec[1].at(10), 300);
}

// ==================== List of Lists ====================

TEST_F(McustlTestFixture, ListOfLists) {
    mcustl::list<mcustl::list<int>> outer;

    mcustl::list<int> inner1;
    inner1.push_back(1); inner1.push_back(2); inner1.push_back(3);
    outer.push_back(inner1);

    mcustl::list<int> inner2;
    inner2.push_back(10); inner2.push_back(20);
    outer.push_back(inner2);

    EXPECT_EQ(outer.size(), 2u);
    EXPECT_EQ(outer.front().size(), 3u);
    EXPECT_EQ(outer.front().front(), 1);
    EXPECT_EQ(outer.front().back(), 3);
    EXPECT_EQ(outer.back().size(), 2u);
    EXPECT_EQ(outer.back().front(), 10);
}

TEST_F(McustlTestFixture, ListOfLists_RemoveOuter) {
    mcustl::list<mcustl::list<int>> outer;

    for (int i = 0; i < 3; i++) {
        mcustl::list<int> inner;
        for (int j = 0; j < 4; j++) {
            inner.push_back(i * 10 + j);
        }
        outer.push_back(inner);
    }

    // Remove first inner list
    outer.pop_front();
    EXPECT_EQ(outer.size(), 2u);
    EXPECT_EQ(outer.front().front(), 10);
    EXPECT_EQ(outer.back().front(), 20);
}

// ==================== List of SmartPtrs ====================

TEST_F(McustlTestFixture, ListOfSmartPtrs) {
    mcustl::list<mcustl::smart_ptr<mcustl::string>> lst;

    for (int i = 0; i < 4; i++) {
        mcustl::smart_ptr<mcustl::string> p;
        const char* words[] = {"one", "two", "three", "four"};
        p.create(words[i]);
        lst.push_back(static_cast<mcustl::smart_ptr<mcustl::string>&&>(p));
    }

    EXPECT_EQ(lst.size(), 4u);
    EXPECT_STREQ(lst.front()->c_str(), "one");
    EXPECT_STREQ(lst.back()->c_str(), "four");

    lst.pop_front();
    EXPECT_STREQ(lst.front()->c_str(), "two");
}

TEST_F(McustlTestFixture, ListInsertEraseSmartPtrs) {
    mcustl::list<mcustl::smart_ptr<int>> lst;

    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        mcustl::smart_ptr<int> p;
        p.allocate(1);
        *p = vals[i];
        lst.push_back(static_cast<mcustl::smart_ptr<int>&&>(p));
    }

    // Insert 15 between 10 and 20
    auto it = lst.begin();
    ++it; // points to 20
    {
        mcustl::smart_ptr<int> p;
        p.allocate(1);
        *p = 15;
        lst.insert(it, static_cast<mcustl::smart_ptr<int>&&>(p));
    }

    // Erase 20 (it still points to 20 since insert is before)
    it = lst.erase(it);

    // Expected: [10, 15, 30]
    EXPECT_EQ(lst.size(), 3u);
    it = lst.begin();
    EXPECT_EQ(**it, 10); ++it;
    EXPECT_EQ(**it, 15); ++it;
    EXPECT_EQ(**it, 30); ++it;
    EXPECT_TRUE(it == lst.end());
}

// ==================== Map of Lists ====================

TEST_F(McustlTestFixture, MapOfLists) {
    mcustl::map<int, mcustl::list<mcustl::string>> m;

    mcustl::list<mcustl::string> fruits;
    fruits.push_back(mcustl::string("apple"));
    fruits.push_back(mcustl::string("banana"));
    m.insert(1, fruits);

    mcustl::list<mcustl::string> vegs;
    vegs.push_back(mcustl::string("carrot"));
    m.insert(2, vegs);

    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(1).size(), 2u);
    EXPECT_STREQ(m.at(1).front().c_str(), "apple");
    EXPECT_STREQ(m.at(2).front().c_str(), "carrot");
}

// ==================== SmartPtr to Map of Strings ====================

TEST_F(McustlTestFixture, SmartPtrToMapOfStrings) {
    mcustl::smart_ptr<mcustl::map<mcustl::string, mcustl::string>> pmap;
    EXPECT_TRUE(pmap.create());

    pmap->insert(mcustl::string("key1"), mcustl::string("val1"));
    pmap->insert(mcustl::string("key2"), mcustl::string("val2"));

    EXPECT_EQ(pmap->size(), 2u);
    EXPECT_STREQ(pmap->at(mcustl::string("key1")).c_str(), "val1");
    EXPECT_STREQ(pmap->at(mcustl::string("key2")).c_str(), "val2");

    pmap.reset();
    EXPECT_EQ(pmap.get(), nullptr);
}

// ==================== 4-level nesting ====================

TEST_F(McustlTestFixture, DeepNesting_SmartPtrToVectorOfMapOfStrings) {
    mcustl::smart_ptr<mcustl::vector<mcustl::map<int, mcustl::string>>> pdata;
    EXPECT_TRUE(pdata.create());

    mcustl::map<int, mcustl::string> row1;
    row1.insert(1, mcustl::string("a"));
    row1.insert(2, mcustl::string("b"));
    pdata->push_back(row1);

    mcustl::map<int, mcustl::string> row2;
    row2.insert(10, mcustl::string("x"));
    pdata->push_back(row2);

    EXPECT_EQ(pdata->size(), 2u);
    EXPECT_STREQ((*pdata)[0].at(1).c_str(), "a");
    EXPECT_STREQ((*pdata)[1].at(10).c_str(), "x");
}

// ==================== Defrag Correctness Tests ====================

TEST_F(McustlTestFixture, Defrag_FreeMiddleAllocation_AllContainersSurvive) {
    // Allocate in order: list, string, map, vector, smart_ptr
    // Then free string (middle) to trigger defrag
    mcustl::list<int> lst;
    lst.push_back(1); lst.push_back(2); lst.push_back(3);

    mcustl::string str("will_be_freed");

    mcustl::map<int, int> m;
    m.insert(10, 100);
    m.insert(20, 200);

    mcustl::vector<int> vec;
    vec.push_back(42); vec.push_back(43);

    mcustl::smart_ptr<int> sp;
    sp.allocate(1);
    *sp = 999;

    // Free the string in the middle
    str.clear();
    str.shrink_to_fit();

    // All others must survive defrag
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.front(), 1);
    EXPECT_EQ(lst.back(), 3);

    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(10), 100);
    EXPECT_EQ(m.at(20), 200);

    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0], 42);
    EXPECT_EQ(vec[1], 43);

    EXPECT_EQ(*sp, 999);
}

TEST_F(McustlTestFixture, Defrag_RepeatedAllocFree_DataIntegrity) {
    // Create and destroy containers in cycles, checking survivors each time
    mcustl::vector<int> survivor;
    for (int i = 0; i < 10; i++) survivor.push_back(i);

    for (int round = 0; round < 10; round++) {
        // Create and destroy a temporary container, triggering defrag
        {
            mcustl::list<int> tmp;
            for (int i = 0; i < 5; i++) tmp.push_back(i * 100);
        }
        {
            mcustl::map<int, int> tmp;
            for (int i = 0; i < 5; i++) tmp.insert(i, i);
        }
        {
            mcustl::string tmp("temporary_string_data");
        }

        // Survivor must remain intact
        ASSERT_EQ(survivor.size(), 10u) << "Failed at round " << round;
        for (int i = 0; i < 10; i++) {
            ASSERT_EQ(survivor[i], i) << "Corruption at round " << round << " index " << i;
        }
    }
}

TEST_F(McustlTestFixture, Defrag_InterleavedAllocFree) {
    // Create A, B, C; free B; check A, C; create D; free A; check C, D
    mcustl::smart_ptr<int> a;
    a.allocate(1); *a = 0xAA;

    mcustl::list<int> b;
    b.push_back(0xBB);

    mcustl::map<int, int> c;
    c.insert(0xCC, 0xCC);

    // Free B
    b.clear();

    EXPECT_EQ(*a, 0xAA);
    EXPECT_EQ(c.at(0xCC), 0xCC);

    // Create D
    mcustl::vector<int> d;
    d.push_back(0xDD);

    // Free A
    a.reset();

    EXPECT_EQ(c.at(0xCC), 0xCC);
    EXPECT_EQ(d[0], 0xDD);
}

TEST_F(McustlTestFixture, Defrag_ListGrowthAfterFrees) {
    // Fill heap with small allocations, free some, then grow a list
    mcustl::smart_ptr<int> p1, p2, p3;
    p1.allocate(1); *p1 = 1;
    p2.allocate(1); *p2 = 2;
    p3.allocate(1); *p3 = 3;

    // Free p2 (middle) - creates hole
    p2.reset();

    // Now grow a list - should work despite fragmentation
    mcustl::list<int> lst;
    for (int i = 0; i < 20; i++) {
        ASSERT_TRUE(lst.push_back(i)) << "Failed to push at i=" << i;
    }

    EXPECT_EQ(*p1, 1);
    EXPECT_EQ(*p3, 3);
    EXPECT_EQ(lst.size(), 20u);
    EXPECT_EQ(lst.front(), 0);
    EXPECT_EQ(lst.back(), 19);
}

TEST_F(McustlTestFixture, Defrag_MapGrowthAfterFrees) {
    mcustl::string s1("first");
    mcustl::string s2("will_free");
    mcustl::string s3("third");

    s2.clear();
    s2.shrink_to_fit();

    mcustl::map<int, int> m;
    for (int i = 0; i < 15; i++) {
        ASSERT_TRUE(m.insert(i, i * 10)) << "Failed to insert at i=" << i;
    }

    EXPECT_STREQ(s1.c_str(), "first");
    EXPECT_STREQ(s3.c_str(), "third");
    EXPECT_EQ(m.size(), 15u);
    for (int i = 0; i < 15; i++) {
        EXPECT_EQ(m.at(i), i * 10);
    }
}

TEST_F(McustlTestFixture, Defrag_IntegrityObjectsSurviveComplexDefrag) {
    IntegrityObject::reset_counters();
    {
        mcustl::vector<IntegrityObject> vec;
        mcustl::list<IntegrityObject> lst;
        mcustl::map<int, IntegrityObject> m;

        for (int i = 0; i < 5; i++) {
            vec.push_back(IntegrityObject(i));
            lst.push_back(IntegrityObject(i + 100));
            m.insert(i, IntegrityObject(i + 200));
        }

        // Remove from middle of each
        vec.pop(2);
        lst.remove(IntegrityObject(102));
        m.erase(2);

        // Verify integrity
        for (uint32_t i = 0; i < vec.size(); i++) {
            EXPECT_TRUE(vec[i].is_valid()) << "Vector corruption at " << i;
        }
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            EXPECT_TRUE(it->is_valid()) << "List corruption";
        }
        for (auto it = m.begin(); it != m.end(); ++it) {
            EXPECT_TRUE(it->second.is_valid()) << "Map corruption";
        }
    }
    EXPECT_TRUE(IntegrityObject::no_errors());
}

TEST_F(McustlTestFixture, Defrag_HeapCleanAfterAllContainersDestroyed) {
    {
        mcustl::vector<int> v;
        v.push_back(1);

        mcustl::string s("test");

        mcustl::list<int> l;
        l.push_back(2);

        mcustl::map<int, int> m;
        m.insert(3, 3);

        mcustl::smart_ptr<int> p;
        p.allocate(1); *p = 4;
    }
    // Heap must be completely clean
    EXPECT_EQ(mcustl_get_default_heap()->alloc_info.allocations_num, 0u);
    EXPECT_EQ(mcustl_get_default_heap()->offset, 0u);
}

TEST_F(McustlTestFixture, Defrag_HeapCleanAfterNestedContainersDestroyed) {
    {
        mcustl::vector<mcustl::list<mcustl::string>> v;

        mcustl::list<mcustl::string> l;
        l.push_back(mcustl::string("hello"));
        l.push_back(mcustl::string("world"));
        v.push_back(l);

        mcustl::map<int, mcustl::vector<int>> m;
        mcustl::vector<int> vv;
        vv.push_back(1); vv.push_back(2);
        m.insert(1, vv);
    }
    EXPECT_EQ(mcustl_get_default_heap()->alloc_info.allocations_num, 0u);
    EXPECT_EQ(mcustl_get_default_heap()->offset, 0u);
}

// ==================== Multithreaded Tests ====================

// Thread-safe test fixture with larger heap for concurrent operations
class McustlThreadedFixture : public ::testing::Test {
protected:
    static constexpr size_t THREADED_HEAP_SIZE = 32768;  // 32KB
    alignas(8) uint8_t heap_memory[THREADED_HEAP_SIZE];

    void SetUp() override {
        mcustl_unregister_heap(true);
        mcustl_register_heap(heap_memory, THREADED_HEAP_SIZE);
        ASSERT_NE(mcustl_get_default_heap(), nullptr);
    }

    void TearDown() override {
        mcustl_unregister_heap(true);
    }
};

TEST_F(McustlThreadedFixture, ConcurrentVectorPushBack) {
    mcustl::vector<int> vec;
    vec.reserve(200);
    std::atomic<int> errors{0};

    auto worker = [&](int start, int count) {
        for (int i = 0; i < count; i++) {
            if (!vec.push_back(start + i)) {
                errors++;
            }
        }
    };

    std::thread t1(worker, 0, 50);
    std::thread t2(worker, 1000, 50);
    t1.join();
    t2.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(vec.size(), 100u);
}

TEST_F(McustlThreadedFixture, ConcurrentListPushBack) {
    mcustl::list<int> lst;
    lst.reserve(200);
    std::atomic<int> errors{0};

    auto worker = [&](int start, int count) {
        for (int i = 0; i < count; i++) {
            if (!lst.push_back(start + i)) {
                errors++;
            }
        }
    };

    std::thread t1(worker, 0, 50);
    std::thread t2(worker, 1000, 50);
    t1.join();
    t2.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(lst.size(), 100u);
}

TEST_F(McustlThreadedFixture, ConcurrentMapInsert) {
    mcustl::map<int, int> m;
    std::atomic<int> errors{0};

    auto worker = [&](int start, int count) {
        for (int i = 0; i < count; i++) {
            if (!m.insert(start + i, (start + i) * 10)) {
                errors++;
            }
        }
    };

    // Non-overlapping key ranges to avoid insert-duplicate false negatives
    std::thread t1(worker, 0, 30);
    std::thread t2(worker, 100, 30);
    t1.join();
    t2.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(m.size(), 60u);

    // Verify all keys present and values correct
    for (int i = 0; i < 30; i++) {
        EXPECT_TRUE(m.contains(i));
        EXPECT_EQ(m.at(i), i * 10);
    }
    for (int i = 100; i < 130; i++) {
        EXPECT_TRUE(m.contains(i));
        EXPECT_EQ(m.at(i), i * 10);
    }
}

TEST_F(McustlThreadedFixture, ConcurrentStringAppend) {
    mcustl::string s1;
    mcustl::string s2;
    std::atomic<int> errors{0};

    auto worker = [&](mcustl::string& s, char ch, int count) {
        for (int i = 0; i < count; i++) {
            if (!s.push_back(ch)) {
                errors++;
            }
        }
    };

    std::thread t1(worker, std::ref(s1), 'A', 50);
    std::thread t2(worker, std::ref(s2), 'B', 50);
    t1.join();
    t2.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(s1.size(), 50u);
    EXPECT_EQ(s2.size(), 50u);
}

TEST_F(McustlThreadedFixture, ConcurrentMixedContainers) {
    // Multiple threads each using different container types on shared heap
    std::atomic<int> errors{0};

    auto vec_worker = [&]() {
        mcustl::vector<int> v;
        for (int i = 0; i < 30; i++) {
            if (!v.push_back(i)) { errors++; return; }
        }
        for (int i = 0; i < 30; i++) {
            if (v[i] != i) { errors++; return; }
        }
    };

    auto list_worker = [&]() {
        mcustl::list<int> l;
        for (int i = 0; i < 30; i++) {
            if (!l.push_back(i * 10)) { errors++; return; }
        }
        if (l.size() != 30) { errors++; return; }
        if (l.front() != 0) { errors++; return; }
        if (l.back() != 290) { errors++; return; }
    };

    auto map_worker = [&]() {
        mcustl::map<int, int> m;
        for (int i = 0; i < 20; i++) {
            if (!m.insert(i, i * 100)) { errors++; return; }
        }
        for (int i = 0; i < 20; i++) {
            if (m.at(i) != i * 100) { errors++; return; }
        }
    };

    auto string_worker = [&]() {
        mcustl::string s("start");
        for (int i = 0; i < 20; i++) {
            s += "x";
        }
        if (s.size() != 25) { errors++; return; }
    };

    std::thread t1(vec_worker);
    std::thread t2(list_worker);
    std::thread t3(map_worker);
    std::thread t4(string_worker);
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_EQ(errors.load(), 0);
}

TEST_F(McustlThreadedFixture, ConcurrentAllocFree) {
    // Threads repeatedly create and destroy containers (stress defrag under contention)
    std::atomic<int> errors{0};

    auto worker = [&](int id) {
        for (int round = 0; round < 10; round++) {
            mcustl::vector<int> v;
            for (int i = 0; i < 10; i++) {
                if (!v.push_back(id * 1000 + round * 100 + i)) {
                    errors++;
                    return;
                }
            }
            // Verify
            for (int i = 0; i < 10; i++) {
                if (v[i] != id * 1000 + round * 100 + i) {
                    errors++;
                    return;
                }
            }
            // v destroyed here, triggers dealloc/defrag
        }
    };

    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    std::thread t3(worker, 3);
    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(errors.load(), 0);
    // Heap should be clean after all threads done
    EXPECT_EQ(mcustl_get_default_heap()->alloc_info.allocations_num, 0u);
}

TEST_F(McustlThreadedFixture, ConcurrentSmartPtrCreateDestroy) {
    std::atomic<int> errors{0};

    auto worker = [&](int id) {
        for (int round = 0; round < 15; round++) {
            mcustl::smart_ptr<int> p;
            if (!p.allocate(1)) { errors++; return; }
            *p = id * 100 + round;
            if (*p != id * 100 + round) { errors++; return; }
        }
    };

    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    t1.join();
    t2.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(mcustl_get_default_heap()->alloc_info.allocations_num, 0u);
}

// ==================== Stress: Complex Nesting Under Load ====================

TEST_F(McustlTestFixture, Stress_MapOfSmartPtrsToStrings) {
    mcustl::map<int, mcustl::smart_ptr<mcustl::string>> m;

    for (int i = 0; i < 8; i++) {
        mcustl::smart_ptr<mcustl::string> p;
        char buf[16];
        snprintf(buf, sizeof(buf), "val_%d", i);
        p.create(buf);
        m.insert(i, static_cast<mcustl::smart_ptr<mcustl::string>&&>(p));
    }

    // Erase every other
    for (int i = 0; i < 8; i += 2) {
        m.erase(i);
    }

    EXPECT_EQ(m.size(), 4u);
    EXPECT_STREQ(m.at(1)->c_str(), "val_1");
    EXPECT_STREQ(m.at(3)->c_str(), "val_3");
    EXPECT_STREQ(m.at(5)->c_str(), "val_5");
    EXPECT_STREQ(m.at(7)->c_str(), "val_7");
}

TEST_F(McustlTestFixture, Stress_VectorOfListsOfStrings) {
    mcustl::vector<mcustl::list<mcustl::string>> vec;

    for (int i = 0; i < 3; i++) {
        mcustl::list<mcustl::string> lst;
        for (int j = 0; j < 3; j++) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d_%d", i, j);
            lst.push_back(mcustl::string(buf));
        }
        vec.push_back(lst);
    }

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_STREQ(vec[0].front().c_str(), "0_0");
    EXPECT_STREQ(vec[2].back().c_str(), "2_2");

    // Pop middle vector element
    vec.pop(1);
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_STREQ(vec[0].front().c_str(), "0_0");
    EXPECT_STREQ(vec[1].front().c_str(), "2_0");
}

/* mcustl::heap_guard smoke tests — verify lock/unlock and reentrancy. */

TEST_F(McustlTestFixture, HeapGuardBasicLockUnlock) {
    {
        mcustl::heap_guard g;
        mcustl::vector<int> v;
        v.push_back(1);
        v.push_back(2);
        EXPECT_EQ(v.size(), 2u);
    }
    mcustl::vector<int> v2;
    v2.push_back(3);
    EXPECT_EQ(v2.size(), 1u);
}

TEST_F(McustlTestFixture, HeapGuardReentrant) {
    /* Nested guards in the same thread must not deadlock — the mutex is
     * recursive. If it ever weren't, this test would hang and gtest
     * would surface it as a timeout. */
    mcustl::heap_guard outer;
    {
        mcustl::heap_guard inner;
        mcustl::vector<int> v;
        v.push_back(42);
        EXPECT_EQ(v[0], 42);
    }
    mcustl::vector<int> v2;
    v2.push_back(7);
    EXPECT_EQ(v2[0], 7);
}

TEST_F(McustlTestFixture, HeapGuardByPointer) {
    /* Same heap as default in single-heap mode, reached via heap_t* ctor. */
    mcustl::heap_guard g(mcustl_get_default_heap());
    mcustl::vector<int> v;
    v.push_back(99);
    EXPECT_EQ(v[0], 99);
}

/* Regression: heap_lock/unlock must auto-fall-back to the default heap
 * when called with a NULL heap pointer. Without that, a container that
 * was default-constructed pre-main (mcustl_register_heap hadn't run yet,
 * so its stored alloc_mem_ptr was NULL) survives by lazily re-binding
 * inside reserve_new_memory — but then the entry-side heap_lock(NULL)
 * is a no-op while the exit-side heap_unlock(now-non-NULL) fires a real
 * give. Net is one extra release per such call.
 *
 * Exciter-mini lived with that for a long time because the give merely
 * lowered the recursive count, never paired with a problem. The bomb
 * went off the first time an outer mcustl::heap_guard wrapped the call
 * (ModifierChain::add taking heap_guard before calling
 * modifiers_.reserve() on the just-created `usb_in` chain): the outer
 * count dropped from 1 to 0 mid-method, the audio thread woke, took
 * heap_guard around modifierChain_.apply(), then waited on ChainLock —
 * which the RPC task still held. Classic AB-BA, hard to spot from logs
 * because both halves looked legitimate.
 *
 * This test reproduces the exact mechanic on the host:
 *   1. force a vector's alloc_mem_ptr to NULL (simulating pre-main ctor)
 *   2. take an outer heap_guard
 *   3. trigger reserve() — the formerly-unbalanced pair
 *   4. ask a foreign thread whether the mutex is still held
 *
 * If reserve() leaked one give, the foreign thread's pthread_mutex_trylock
 * succeeds and the test fails. With the fallback in heap_lock, both
 * sides operate on the default heap and the pair stays balanced.
 *
 * Same shape covers map / string / list because they all funnel
 * through the same heap_lock/heap_unlock helpers. */
TEST_F(McustlTestFixture, HeapGuardSurvivesLazyInitInReserve) {
    mcustl::vector<int> vec;
    vec.assign_mem_pointer(nullptr);
    ASSERT_EQ(vec.get_mem_pointer(), nullptr)
        << "Test setup: expected NULL heap ptr on the vector";

    pthread_mutex_t* m = mcustl_get_default_heap()->mutex;
    ASSERT_NE(m, nullptr);

    mcustl::heap_guard outer;   // outer take: count 0 -> 1, holder = this thread
    ASSERT_TRUE(vec.reserve(8));
    EXPECT_NE(vec.get_mem_pointer(), nullptr)
        << "reserve() should have lazy-bound the heap ptr";

    /* Foreign thread: trylock from a non-owning context. With the bug
     * the mutex was silently surrendered during reserve() and a
     * foreign thread could grab it. With the fix it stays ours. */
    std::atomic<bool> foreign_got_lock{false};
    std::thread other([m, &foreign_got_lock]() {
        if (pthread_mutex_trylock(m) == 0) {
            foreign_got_lock = true;
            pthread_mutex_unlock(m);
        }
    });
    other.join();

    EXPECT_FALSE(foreign_got_lock.load())
        << "After vector::reserve() on a NULL-heap vector, the outer "
        << "heap_guard's mutex was prematurely released — that is the "
        << "exciter-mini bt_task/stream_task AB-BA deadlock root cause";

    /* Sanity: vector still functional through the formerly-broken path. */
    EXPECT_TRUE(vec.push_back(42));
    EXPECT_EQ(vec[0], 42);
}

/* Same balance check for the other lazy-init entry point — push_back on
 * an empty vector also routes through reserve_new_memory. Cheap to
 * cover separately so a future change that only patches reserve()
 * doesn't silently regress push_back. */
TEST_F(McustlTestFixture, HeapGuardSurvivesLazyInitInPushBack) {
    mcustl::vector<int> vec;
    vec.assign_mem_pointer(nullptr);
    ASSERT_EQ(vec.get_mem_pointer(), nullptr);

    pthread_mutex_t* m = mcustl_get_default_heap()->mutex;
    ASSERT_NE(m, nullptr);

    mcustl::heap_guard outer;
    ASSERT_TRUE(vec.push_back(7));

    std::atomic<bool> foreign_got_lock{false};
    std::thread other([m, &foreign_got_lock]() {
        if (pthread_mutex_trylock(m) == 0) {
            foreign_got_lock = true;
            pthread_mutex_unlock(m);
        }
    });
    other.join();

    EXPECT_FALSE(foreign_got_lock.load())
        << "push_back() on NULL-heap vector silently released the outer "
        << "heap_guard's mutex";

    EXPECT_EQ(vec.size(), 1u);
    EXPECT_EQ(vec[0], 7);
}

TEST_F(McustlTestFixture, Stress_AllContainersRepeatedCreateDestroy) {
    for (int round = 0; round < 5; round++) {
        {
            mcustl::vector<mcustl::string> vs;
            vs.push_back(mcustl::string("vec_str"));

            mcustl::list<mcustl::map<int, int>> lm;
            mcustl::map<int, int> m;
            m.insert(1, 1);
            lm.push_back(m);

            mcustl::smart_ptr<mcustl::list<int>> sl;
            sl.create();
            sl->push_back(42);

            mcustl::map<int, mcustl::string> ms;
            ms.insert(1, mcustl::string("map_str"));
        }
        // All destroyed — heap must be clean
        ASSERT_EQ(mcustl_get_default_heap()->alloc_info.allocations_num, 0u)
            << "Leak at round " << round;
        ASSERT_EQ(mcustl_get_default_heap()->offset, 0u)
            << "Heap not reset at round " << round;
    }
}
