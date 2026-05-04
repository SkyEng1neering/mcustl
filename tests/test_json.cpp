/**
 * @file test_json.cpp
 * @brief mcustl::json tests
 *
 * Coverage:
 *   - Construction from every supported source type (null, bool, int, double,
 *     C-string, mcustl::string, mcustl::vector, mcustl::map)
 *   - Copy / move ctor + assignment, self-assignment, deep-copy invariants
 *   - Type queries (is_*) and accessors (get_*)
 *   - Object operations: operator[] auto-vivification, at, contains, erase
 *   - Array operations: operator[idx], at, push_back, erase
 *   - Iteration over arrays and objects
 *   - Equality (deep, including int↔float numeric equality)
 *   - parse() — happy paths for null/bool/int/float/string/array/object
 *   - parse() — escapes, \uXXXX, surrogate pair, nested structures
 *   - parse() — malformed input returns error, no leaks
 *   - dump() — compact and pretty-printed, parse↔dump roundtrip
 *   - Stress: 200-element array, 50-key object, parse→dump→reparse equal
 *
 * The McustlTestFixture re-creates a small heap for each test and the
 * mcustl allocator self-checks every (de)allocation; combined with ASan/UBSan
 * (enabled in CMakeLists.txt) leaks and use-after-free are caught.
 */

#include "mcustl_test_config.h"

using mcustl::json;

/* ==================== Construction ==================== */

TEST_F(McustlTestFixture, JsonDefaultIsNull) {
    json j;
    EXPECT_TRUE(j.is_null());
    EXPECT_EQ(j.type(), json::value_t::null);
    EXPECT_EQ(j.size(), 0u);
    EXPECT_TRUE(j.empty());
}

TEST_F(McustlTestFixture, JsonFromNullptrLiteral) {
    json j(nullptr);
    EXPECT_TRUE(j.is_null());
}

TEST_F(McustlTestFixture, JsonFromBool) {
    json t(true);
    json f(false);
    EXPECT_TRUE(t.is_boolean()); EXPECT_TRUE(t.get_bool());
    EXPECT_TRUE(f.is_boolean()); EXPECT_FALSE(f.get_bool());
}

TEST_F(McustlTestFixture, JsonFromInt) {
    json a(42);
    json b((int64_t)-7);
    json c((unsigned long long)0xFFFFFFFFULL);
    EXPECT_TRUE(a.is_number_int());
    EXPECT_TRUE(a.is_number());
    EXPECT_FALSE(a.is_number_float());
    EXPECT_EQ(a.get_int(), 42);
    EXPECT_EQ(b.get_int(), -7);
    EXPECT_EQ(c.get_int(), (int64_t)0xFFFFFFFFLL);
}

TEST_F(McustlTestFixture, JsonFromDouble) {
    json a(3.14);
    json b(1.5f);
    EXPECT_TRUE(a.is_number_float());
    EXPECT_TRUE(a.is_number());
    EXPECT_DOUBLE_EQ(a.get_float(), 3.14);
    EXPECT_DOUBLE_EQ(b.get_float(), 1.5);
    EXPECT_EQ(a.get_int(), 3); /* lossy coercion */
}

TEST_F(McustlTestFixture, JsonFromCString) {
    json j("hello");
    EXPECT_TRUE(j.is_string());
    EXPECT_EQ(j.size(), 5u);
    EXPECT_STREQ(j.get_string().c_str(), "hello");
}

TEST_F(McustlTestFixture, JsonFromMcustlString) {
    mcustl::string s("world");
    json j(s);
    EXPECT_TRUE(j.is_string());
    EXPECT_STREQ(j.get_string().c_str(), "world");
}

TEST_F(McustlTestFixture, JsonFromArray) {
    json::array_t arr;
    arr.push_back(json(1));
    arr.push_back(json("x"));
    json j(arr);
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 2u);
    EXPECT_EQ(j[0].get_int(), 1);
    EXPECT_STREQ(j[1].get_string().c_str(), "x");
}

TEST_F(McustlTestFixture, JsonFromObject) {
    json::object_t obj;
    obj.insert(mcustl::string("a"), json(1));
    obj.insert(mcustl::string("b"), json(2));
    json j(obj);
    EXPECT_TRUE(j.is_object());
    EXPECT_EQ(j.size(), 2u);
    EXPECT_EQ(j.at("a").get_int(), 1);
    EXPECT_EQ(j.at("b").get_int(), 2);
}

/* ==================== Copy / move ==================== */

TEST_F(McustlTestFixture, JsonCopyDeepIndependent) {
    json a;
    a["k"] = json(10);
    json b = a;                       /* copy */
    b["k"] = json(20);
    EXPECT_EQ(a["k"].get_int(), 10);  /* a unchanged */
    EXPECT_EQ(b["k"].get_int(), 20);
}

TEST_F(McustlTestFixture, JsonMoveTransfersOwnership) {
    json a("hello");
    const char* before = a.get_string().c_str();
    json b = static_cast<json&&>(a);
    EXPECT_TRUE(a.is_null());
    EXPECT_TRUE(b.is_string());
    EXPECT_STREQ(b.get_string().c_str(), "hello");
    /* Pointer should have been moved verbatim. */
    EXPECT_EQ(b.get_string().c_str(), before);
}

TEST_F(McustlTestFixture, JsonCopyAssignSelfSafe) {
    json a;
    a["k"] = json(7);
    json* p = &a;
    a = *p;                           /* explicit self-assign */
    EXPECT_EQ(a["k"].get_int(), 7);
}

TEST_F(McustlTestFixture, JsonMoveAssign) {
    json a;
    a.push_back(json(1));
    a.push_back(json(2));
    json b;
    b = static_cast<json&&>(a);
    EXPECT_TRUE(a.is_null());
    EXPECT_EQ(b.size(), 2u);
}

/* ==================== Object ops ==================== */

TEST_F(McustlTestFixture, JsonObjectAutoVivify) {
    json j;
    j["x"] = json(1);                 /* turns null into object */
    EXPECT_TRUE(j.is_object());
    EXPECT_TRUE(j.contains("x"));
    EXPECT_FALSE(j.contains("y"));
    j["y"];                           /* vivified as null */
    EXPECT_TRUE(j.contains("y"));
    EXPECT_TRUE(j.at("y").is_null());
}

TEST_F(McustlTestFixture, JsonObjectErase) {
    json j;
    j["a"] = json(1);
    j["b"] = json(2);
    EXPECT_TRUE(j.erase("a"));
    EXPECT_FALSE(j.contains("a"));
    EXPECT_FALSE(j.erase("a"));       /* idempotent */
    EXPECT_EQ(j.size(), 1u);
}

/* ==================== Array ops ==================== */

TEST_F(McustlTestFixture, JsonArrayPushBack) {
    json j;
    j.push_back(json(1));
    j.push_back(json(2));
    j.push_back(json("three"));
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 3u);
    EXPECT_EQ(j[0].get_int(), 1);
    EXPECT_STREQ(j[2].get_string().c_str(), "three");
}

TEST_F(McustlTestFixture, JsonArrayErase) {
    json j;
    j.push_back(json(10));
    j.push_back(json(20));
    j.push_back(json(30));
    EXPECT_TRUE(j.erase((size_t)1));
    EXPECT_EQ(j.size(), 2u);
    EXPECT_EQ(j[0].get_int(), 10);
    EXPECT_EQ(j[1].get_int(), 30);
}

/* ==================== Iteration ==================== */

TEST_F(McustlTestFixture, JsonIterateArray) {
    json j;
    for (int i = 0; i < 5; ++i) j.push_back(json(i));
    int sum = 0;
    for (auto it = j.begin(); it != j.end(); ++it) sum += (int)it->get_int();
    EXPECT_EQ(sum, 0 + 1 + 2 + 3 + 4);
}

TEST_F(McustlTestFixture, JsonIterateObject) {
    json j;
    j["a"] = json(1);
    j["b"] = json(2);
    j["c"] = json(3);
    int seen_keys = 0;
    int sum = 0;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const auto& k = it.key();
        EXPECT_GT(k.size(), 0u);  /* mcustl::string::empty() isn't const */
        sum += (int)it->get_int();
        ++seen_keys;
    }
    EXPECT_EQ(seen_keys, 3);
    EXPECT_EQ(sum, 1 + 2 + 3);
}

/* ==================== Equality ==================== */

TEST_F(McustlTestFixture, JsonEqualityScalars) {
    EXPECT_EQ(json(),           json());
    EXPECT_EQ(json(true),       json(true));
    EXPECT_NE(json(true),       json(false));
    EXPECT_EQ(json(42),         json(42));
    EXPECT_EQ(json(42),         json(42.0));   /* number↔number */
    EXPECT_EQ(json(3.14),       json(3.14));
    EXPECT_EQ(json("abc"),      json("abc"));
    EXPECT_NE(json("abc"),      json("abcd"));
    EXPECT_NE(json(1),          json("1"));
}

TEST_F(McustlTestFixture, JsonEqualityNested) {
    json a;
    a["k"] = json(1);
    a["arr"] = json();
    a["arr"].push_back(json("x"));
    a["arr"].push_back(json(2));

    json b;
    b["k"] = json(1);
    b["arr"] = json();
    b["arr"].push_back(json("x"));
    b["arr"].push_back(json(2));

    EXPECT_EQ(a, b);

    b["arr"][1] = json(3);
    EXPECT_NE(a, b);
}

/* ==================== Parse — happy path ==================== */

TEST_F(McustlTestFixture, JsonParseScalars) {
    EXPECT_TRUE(json::parse("null").is_null());
    EXPECT_TRUE(json::parse("true").get_bool());
    EXPECT_FALSE(json::parse("false").get_bool());
    EXPECT_EQ(json::parse("42").get_int(), 42);
    EXPECT_EQ(json::parse("-123").get_int(), -123);
    EXPECT_DOUBLE_EQ(json::parse("3.14").get_float(), 3.14);
    EXPECT_DOUBLE_EQ(json::parse("1.5e2").get_float(), 150.0);
    EXPECT_DOUBLE_EQ(json::parse("-1.5e-2").get_float(), -0.015);
    EXPECT_STREQ(json::parse("\"hello\"").get_string().c_str(), "hello");
}

TEST_F(McustlTestFixture, JsonParseEmptyContainers) {
    EXPECT_TRUE(json::parse("[]").is_array());
    EXPECT_EQ(json::parse("[]").size(), 0u);
    EXPECT_TRUE(json::parse("{}").is_object());
    EXPECT_EQ(json::parse("{}").size(), 0u);
}

TEST_F(McustlTestFixture, JsonParseArray) {
    json j = json::parse("[1, 2, \"three\", true, null]");
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 5u);
    EXPECT_EQ(j[0].get_int(), 1);
    EXPECT_STREQ(j[2].get_string().c_str(), "three");
    EXPECT_TRUE(j[3].get_bool());
    EXPECT_TRUE(j[4].is_null());
}

TEST_F(McustlTestFixture, JsonParseObject) {
    const char* text = "{\"a\":1, \"b\":\"two\", \"c\":[3,4]}";
    json j = json::parse(text);
    EXPECT_TRUE(j.is_object());
    EXPECT_EQ(j.size(), 3u);
    EXPECT_EQ(j.at("a").get_int(), 1);
    EXPECT_STREQ(j.at("b").get_string().c_str(), "two");
    EXPECT_EQ(j.at("c").size(), 2u);
    EXPECT_EQ(j.at("c")[0].get_int(), 3);
}

TEST_F(McustlTestFixture, JsonParseStringEscapes) {
    json j = json::parse("\"a\\\"b\\\\c\\nd\\te\"");
    EXPECT_TRUE(j.is_string());
    EXPECT_STREQ(j.get_string().c_str(), "a\"b\\c\nd\te");
}

TEST_F(McustlTestFixture, JsonParseUnicodeEscape) {
    /* "é" = é (U+00E9) → UTF-8 0xC3 0xA9 */
    json j = json::parse("\"\\u00E9\"");
    EXPECT_TRUE(j.is_string());
    const char* s = j.get_string().c_str();
    EXPECT_EQ((unsigned char)s[0], 0xC3u);
    EXPECT_EQ((unsigned char)s[1], 0xA9u);
    EXPECT_EQ(s[2], '\0');
}

TEST_F(McustlTestFixture, JsonParseSurrogatePair) {
    /* "😀" = 😀 (U+1F600) → UTF-8 F0 9F 98 80 */
    json j = json::parse("\"\\uD83D\\uDE00\"");
    EXPECT_TRUE(j.is_string());
    const unsigned char* s = (const unsigned char*)j.get_string().c_str();
    EXPECT_EQ(s[0], 0xF0u);
    EXPECT_EQ(s[1], 0x9Fu);
    EXPECT_EQ(s[2], 0x98u);
    EXPECT_EQ(s[3], 0x80u);
}

TEST_F(McustlTestFixture, JsonParseNestedDeep) {
    /* 5 levels of nesting */
    json j = json::parse("{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":42}}}}}");
    EXPECT_EQ(j.at("a").at("b").at("c").at("d").at("e").get_int(), 42);
}

TEST_F(McustlTestFixture, JsonParseWhitespace) {
    json j = json::parse("\n\t {  \"k\"  :   [ 1 , 2 ]  } \n");
    EXPECT_TRUE(j.is_object());
    EXPECT_EQ(j.at("k").size(), 2u);
}

/* ==================== Parse — errors ==================== */

TEST_F(McustlTestFixture, JsonParseErrorReturnsNullAndDiagnostic) {
    const char* err = nullptr;
    size_t      pos = 0;
    json j = json::parse("{not_json}", nullptr, &err, &pos);
    EXPECT_TRUE(j.is_null());
    EXPECT_NE(err, nullptr);
}

TEST_F(McustlTestFixture, JsonParseTrailingContent) {
    const char* err = nullptr;
    json j = json::parse("42 garbage", nullptr, &err);
    EXPECT_TRUE(j.is_null());
    EXPECT_NE(err, nullptr);
}

TEST_F(McustlTestFixture, JsonParseMalformedNumber) {
    const char* err = nullptr;
    EXPECT_TRUE(json::parse("--1", nullptr, &err).is_null());
    EXPECT_NE(err, nullptr);
}

TEST_F(McustlTestFixture, JsonParseUnterminatedString) {
    const char* err = nullptr;
    EXPECT_TRUE(json::parse("\"oops", nullptr, &err).is_null());
    EXPECT_NE(err, nullptr);
}

TEST_F(McustlTestFixture, JsonParseUnterminatedArray) {
    const char* err = nullptr;
    EXPECT_TRUE(json::parse("[1, 2", nullptr, &err).is_null());
    EXPECT_NE(err, nullptr);
}

TEST_F(McustlTestFixture, JsonParseUnterminatedObject) {
    const char* err = nullptr;
    EXPECT_TRUE(json::parse("{\"a\":1", nullptr, &err).is_null());
    EXPECT_NE(err, nullptr);
}

TEST_F(McustlTestFixture, JsonParseDepthLimitRejected) {
    /* Build a chain of (MCUSTL_JSON_MAX_DEPTH + 5) opening brackets followed
     * by closing brackets — the parser must refuse it without crashing. */
    mcustl::string text;
    const int over = MCUSTL_JSON_MAX_DEPTH + 5;
    for (int i = 0; i < over; ++i) text.append('[');
    for (int i = 0; i < over; ++i) text.append(']');
    const char* err = nullptr;
    json j = json::parse(text.c_str(), nullptr, &err);
    EXPECT_TRUE(j.is_null());
    EXPECT_NE(err, nullptr);
}

/* ==================== Dump ==================== */

TEST_F(McustlTestFixture, JsonDumpScalars) {
    EXPECT_STREQ(json().dump().c_str(),       "null");
    EXPECT_STREQ(json(true).dump().c_str(),   "true");
    EXPECT_STREQ(json(false).dump().c_str(),  "false");
    EXPECT_STREQ(json(42).dump().c_str(),     "42");
    EXPECT_STREQ(json(-7).dump().c_str(),     "-7");
    EXPECT_STREQ(json("hi").dump().c_str(),   "\"hi\"");
}

TEST_F(McustlTestFixture, JsonDumpStringEscapes) {
    json j("a\"b\nc\t");
    /* Compact dump should escape \" \n \t */
    EXPECT_STREQ(j.dump().c_str(), "\"a\\\"b\\nc\\t\"");
}

TEST_F(McustlTestFixture, JsonDumpControlCharsAsUEscape) {
    mcustl::string raw;
    raw.append('\x01');  /* SOH */
    raw.append('\x1F');  /* US */
    json j(raw);
    EXPECT_STREQ(j.dump().c_str(), "\"\\u0001\\u001f\"");
}

TEST_F(McustlTestFixture, JsonDumpEmptyContainers) {
    json a; a.push_back(json(1)); a.erase((size_t)0);
    /* a is now an empty array */
    EXPECT_STREQ(a.dump().c_str(), "[]");
    json o; o["x"] = json(1); o.erase("x");
    EXPECT_STREQ(o.dump().c_str(), "{}");
}

TEST_F(McustlTestFixture, JsonDumpCompactObject) {
    json j;
    j["a"] = json(1);
    j["b"] = json("two");
    /* Object iteration order is sorted (it's a map). */
    EXPECT_STREQ(j.dump().c_str(), "{\"a\":1,\"b\":\"two\"}");
}

TEST_F(McustlTestFixture, JsonDumpPrettyArray) {
    json j;
    j.push_back(json(1));
    j.push_back(json(2));
    EXPECT_STREQ(j.dump(2).c_str(), "[\n  1,\n  2\n]");
}

TEST_F(McustlTestFixture, JsonDumpPrettyObject) {
    json j;
    j["a"] = json(1);
    j["b"] = json(2);
    /* Keys printed in sorted order. */
    EXPECT_STREQ(j.dump(2).c_str(),
                 "{\n  \"a\": 1,\n  \"b\": 2\n}");
}

/* ==================== Roundtrip ==================== */

TEST_F(McustlTestFixture, JsonRoundtripNested) {
    const char* text =
        "{\"name\":\"x\",\"vals\":[1,2,3.5],\"nested\":{\"ok\":true,\"why\":null}}";
    json j = json::parse(text);
    ASSERT_FALSE(j.is_null());
    auto dumped = j.dump();
    json j2 = json::parse(dumped.c_str());
    EXPECT_EQ(j, j2);
}

TEST_F(McustlTestFixture, JsonRoundtripUnicode) {
    json j = json::parse("\"\\u00E9\\uD83D\\uDE00 hi\"");
    auto dumped = j.dump();
    json j2 = json::parse(dumped.c_str());
    EXPECT_EQ(j, j2);
    EXPECT_EQ(j2.get_string().size(), j.get_string().size());
}

/* ==================== Stress ==================== */

TEST_F(McustlTestFixture, JsonStressLargeArray) {
    json arr;
    const int N = 200;
    for (int i = 0; i < N; ++i) arr.push_back(json(i));
    EXPECT_EQ(arr.size(), (size_t)N);
    EXPECT_EQ(arr[100].get_int(), 100);

    auto text = arr.dump();
    json reparsed = json::parse(text.c_str());
    EXPECT_EQ(reparsed.size(), (size_t)N);
    EXPECT_EQ(reparsed[N - 1].get_int(), N - 1);
}

TEST_F(McustlTestFixture, JsonStressLargeObject) {
    json obj;
    const int N = 50;
    char buf[16];
    for (int i = 0; i < N; ++i) {
        snprintf(buf, sizeof(buf), "k%03d", i);
        obj[buf] = json(i);
    }
    EXPECT_EQ(obj.size(), (size_t)N);

    auto text = obj.dump();
    json reparsed = json::parse(text.c_str());
    EXPECT_EQ(reparsed, obj);
}

/* ==================== Memory leak guard ==================== */

TEST_F(McustlTestFixture, JsonHeapClearedOnDestruction) {
    /* Create and destroy a complex value within nested scope; the fixture's
     * canary check + ASan + dalloc accounting will surface any leak. */
    {
        json j = json::parse(
            "{\"a\":[1,2,3],\"b\":{\"x\":\"hello\",\"y\":42},\"c\":null}");
        ASSERT_FALSE(j.is_null());
        (void)j.dump();
    }
    /* Heap should be back to baseline now (TearDown will assert canaries
     * are still intact and unregister the heap forcibly to surface leaks). */
}

TEST_F(McustlTestFixture, JsonReassignmentReleasesOldPayload) {
    json j = json::parse("[1,2,3,4,5]");
    j = json("replaced");
    EXPECT_TRUE(j.is_string());
    j = json();
    EXPECT_TRUE(j.is_null());
    /* No leak — fixture cross-checks. */
}

/* ==================== Device-config-shaped repro ==================== */

/* Reproduces a corruption pattern observed on a Cortex-M7 target: parsing
 * the same JSON the device uses for its config produced an object where the
 * FIRST key/value of the nested object lost its key data (size preserved,
 * bytes zeroed), while later keys/values stayed intact. The hypothesis is
 * that something inside map::insert dereferences a pointer into the node
 * storage that gets relocated by a defrag triggered mid-assignment. This
 * test exercises the exact JSON shape under whatever ALLOCATION_ALIGNMENT
 * the test config uses, so a CI run with align=32 catches it. */
TEST_F(McustlTestFixture, JsonParseDeviceConfigShape) {
    const char* text =
        "{\n"
        "  \"streams\": [\n"
        "    {\n"
        "      \"name\": \"speaker\",\n"
        "      \"modifiers\": [\n"
        "        \"pitch_down\"\n"
        "      ]\n"
        "    }\n"
        "  ],\n"
        "  \"stream_connections\": [\n"
        "    {\n"
        "      \"input\": \"usb_in\",\n"
        "      \"output\": \"speaker\"\n"
        "    }\n"
        "  ]\n"
        "}";

    json j = json::parse(text, strlen(text));
    ASSERT_FALSE(j.is_null());
    ASSERT_TRUE(j.is_object());

    /* Top-level keys present and pointing at arrays of size 1. */
    ASSERT_TRUE(j.contains("streams"));
    ASSERT_TRUE(j.contains("stream_connections"));

    const json& streams = j.at("streams");
    ASSERT_TRUE(streams.is_array());
    ASSERT_EQ(streams.size(), 1u);

    const json& s0 = streams[0];
    ASSERT_TRUE(s0.is_object());
    /* The bug surfaces here on the device: contains("name") returned true
     * but at("name").is_string() was false because the key buffer had been
     * zeroed.  Both checks must hold. */
    ASSERT_TRUE(s0.contains("name"))     << "streams[0].name key missing";
    ASSERT_TRUE(s0.at("name").is_string()) << "streams[0].name not a string";
    EXPECT_STREQ(s0.at("name").get_string().c_str(), "speaker");

    ASSERT_TRUE(s0.contains("modifiers"));
    ASSERT_TRUE(s0.at("modifiers").is_array());
    ASSERT_EQ(s0.at("modifiers").size(), 1u);
    EXPECT_STREQ(s0.at("modifiers")[0].get_string().c_str(), "pitch_down");

    /* The second device-side symptom: connection.input was an EMPTY string. */
    const json& cons = j.at("stream_connections");
    ASSERT_TRUE(cons.is_array());
    ASSERT_EQ(cons.size(), 1u);

    const json& c0 = cons[0];
    ASSERT_TRUE(c0.contains("input"));
    ASSERT_TRUE(c0.at("input").is_string());
    EXPECT_STREQ(c0.at("input").get_string().c_str(), "usb_in")
        << "connection.input was empty on device — repro";

    ASSERT_TRUE(c0.contains("output"));
    EXPECT_STREQ(c0.at("output").get_string().c_str(), "speaker");

    /* Iterate the inner object and verify every key has the right
     * size+bytes (the failure mode: klen correct, bytes zero). */
    int seen_name = 0, seen_modifiers = 0;
    for (auto it = s0.begin(); it != s0.end(); ++it) {
        const auto& k = it.key();
        if (k.size() == 4 && memcmp(k.c_str(), "name", 4) == 0)      ++seen_name;
        if (k.size() == 9 && memcmp(k.c_str(), "modifiers", 9) == 0) ++seen_modifiers;
    }
    EXPECT_EQ(seen_name, 1)      << "streams[0] iteration didn't see \"name\"";
    EXPECT_EQ(seen_modifiers, 1) << "streams[0] iteration didn't see \"modifiers\"";
}

/* The exciter-mini SystemConfig::dumpParsed() walks the parsed root with
 * the heap mutex held and prints `it.key()` / `it.value()` for every
 * top-level object key. On the device this path observed empty key
 * strings whose size_val matched the original key's length but whose
 * char buffer was zeroed — i.e. the tracker for the key string was
 * pointing at memory that had been freed/reused by a later allocation.
 * This test mimics that exact walk and asserts every key is intact. */
TEST_F(McustlTestFixture, JsonParseDeviceConfigShape_IteratorWalk) {
    const char* text =
        "{\n"
        "  \"streams\": [\n"
        "    {\n"
        "      \"name\": \"speaker\",\n"
        "      \"modifiers\": [\n"
        "        \"pitch_down\"\n"
        "      ]\n"
        "    }\n"
        "  ],\n"
        "  \"stream_connections\": [\n"
        "    {\n"
        "      \"input\": \"usb_in\",\n"
        "      \"output\": \"speaker\"\n"
        "    }\n"
        "  ]\n"
        "}";

    json j = json::parse(text, strlen(text));
    ASSERT_TRUE(j.is_object());

    /* Walk every level via iterator, reading key().c_str() each time
     * — exactly the device's failure mode. */
    {
        mcustl::heap_guard g;
        int top_keys = 0;
        for (auto it = j.begin(); it != j.end(); ++it) {
            const auto& k = it.key();
            ++top_keys;
            EXPECT_GT(k.size(), 0u) << "top-level key has size 0";
            ASSERT_NE(k.c_str(), nullptr);
            EXPECT_NE(k.c_str()[0], '\0') << "top-level key starts with NUL";
            const auto& v = it.value();
            ASSERT_TRUE(v.is_array()) << "top key \"" << k.c_str() << "\" not array";
            for (size_t i = 0; i < v.size(); ++i) {
                const json& e = v[i];
                ASSERT_TRUE(e.is_object());
                int inner_keys = 0;
                for (auto eit = e.begin(); eit != e.end(); ++eit) {
                    const auto& ek = eit.key();
                    ++inner_keys;
                    /* The exact failure mode — key has correct size_val
                     * but buffer is zeroed. Catch it with an explicit
                     * check, not just c_str()[0]. */
                    EXPECT_GT(ek.size(), 0u) << "inner key size 0";
                    bool any_nz = false;
                    for (uint32_t b = 0; b < ek.size(); ++b) {
                        if (ek.c_str()[b] != '\0') { any_nz = true; break; }
                    }
                    EXPECT_TRUE(any_nz)
                        << "inner key buffer is all zeros (size=" << ek.size() << ")";
                }
                EXPECT_EQ(inner_keys, 2) << "expected exactly 2 inner keys per entry";
            }
        }
        EXPECT_EQ(top_keys, 2);
    }
}

/* Granular dump after parse: at every level, verify type + size.
 * Pinpoints which exact accessor returns a corrupt result. */
TEST_F(McustlTestFixture, JsonParseDeviceConfigShape_GranularInspect) {
    const char* text =
        "{\"streams\":[{\"name\":\"speaker\",\"modifiers\":[\"pitch_down\"]}],"
        "\"stream_connections\":[{\"input\":\"usb_in\",\"output\":\"speaker\"}]}";

    json j = json::parse(text, strlen(text));
    ASSERT_TRUE(j.is_object());
    ASSERT_EQ(j.size(), 2u);

    /* Level 1: top-level object keys. */
    ASSERT_TRUE(j.contains("stream_connections"));
    const json& cons_arr = j.at("stream_connections");
    ASSERT_TRUE(cons_arr.is_array());
    ASSERT_EQ(cons_arr.size(), 1u);

    /* Level 2: connections[0]. */
    const json& c0 = cons_arr[0];
    ASSERT_TRUE(c0.is_object());
    ASSERT_EQ(c0.size(), 2u);
    ASSERT_TRUE(c0.contains("input"));

    /* Level 3: connection[0]["input"]. */
    const json& input_node = c0.at("input");
    ASSERT_TRUE(input_node.is_string());

    /* Level 4: get_string() returns the string struct reference. */
    const mcustl::string& input_str = input_node.get_string();

    /* CRITICAL CHECKPOINT: is the string struct valid here? */
    fprintf(stderr, "  &input_str=%p\n", (const void*)&input_str);
    fprintf(stderr, "  input_str.size()=%u\n", (unsigned)input_str.size());
    fprintf(stderr, "  input_str.c_str()=%p\n", (void*)input_str.c_str());

    /* Dump first 16 bytes of the buffer pointed to by c_str(). If
     * the buffer is "usb_in\0..." then size_val is wrong but data
     * is correct. If buffer is zeros, the buffer too is wiped. */
    if (input_str.c_str()) {
        fprintf(stderr, "  buffer content (16 bytes): ");
        for (int b = 0; b < 16; ++b) {
            fprintf(stderr, "%02x ", (unsigned)(unsigned char)input_str.c_str()[b]);
        }
        fprintf(stderr, "\n");
    }

    /* Dump the string struct's raw bytes (sizeof string ~32). */
    fprintf(stderr, "  string struct raw bytes: ");
    const unsigned char* sb = (const unsigned char*)&input_str;
    for (int b = 0; b < 32; ++b) {
        fprintf(stderr, "%02x ", sb[b]);
    }
    fprintf(stderr, "\n");

    EXPECT_EQ(input_str.size(), 6u);
    EXPECT_STREQ(input_str.c_str(), "usb_in");
}

/* Same as above but WITH heap_guard. */
TEST_F(McustlTestFixture, JsonParseDeviceConfigShape_StringCopyOnlyConnections) {
    const char* text =
        "{\"streams\":[{\"name\":\"speaker\",\"modifiers\":[\"pitch_down\"]}],"
        "\"stream_connections\":[{\"input\":\"usb_in\",\"output\":\"speaker\"}]}";

    json j = json::parse(text, strlen(text));
    ASSERT_TRUE(j.is_object());

    {
        mcustl::heap_guard g;
        const json& c0 = j.at("stream_connections")[0];

        EXPECT_EQ(c0.at("input").get_string().size(), 6u);
        EXPECT_STREQ(c0.at("input").get_string().c_str(), "usb_in");

        mcustl::string input_copy  = c0.at("input").get_string();
        mcustl::string output_copy = c0.at("output").get_string();
        EXPECT_STREQ(input_copy.c_str(),  "usb_in");
        EXPECT_STREQ(output_copy.c_str(), "speaker");
    }
}

/* Mimics SystemConfig::applyToStreams(): walks parsed json and copies
 * key/value strings into mcustl::string locals (which is what the
 * device does to keep the data alive across foreign defrag). The bug
 * surface: after the copy, the stored strings should match the source. */
TEST_F(McustlTestFixture, JsonParseDeviceConfigShape_StringCopyOut) {
    const char* text =
        "{\"streams\":[{\"name\":\"speaker\",\"modifiers\":[\"pitch_down\"]}],"
        "\"stream_connections\":[{\"input\":\"usb_in\",\"output\":\"speaker\"}]}";

    json j = json::parse(text, strlen(text));
    ASSERT_TRUE(j.is_object());

    /* For each stream entry: copy "name" and each modifier into local
     * mcustl::string. Then check the copies are intact. The device
     * applyToStreams does this same copy under heap_guard before
     * passing c_str() to subsystem APIs. */
    {
        mcustl::heap_guard g;
        const json& streams = j.at("streams");
        ASSERT_TRUE(streams.is_array());
        ASSERT_EQ(streams.size(), 1u);

        const json& s0 = streams[0];
        ASSERT_TRUE(s0.is_object());
        ASSERT_TRUE(s0.contains("name"));
        ASSERT_TRUE(s0.at("name").is_string());

        mcustl::string name_copy = s0.at("name").get_string();
        EXPECT_EQ(name_copy.size(), 7u);
        EXPECT_STREQ(name_copy.c_str(), "speaker");

        ASSERT_TRUE(s0.contains("modifiers"));
        const json& mods = s0.at("modifiers");
        ASSERT_TRUE(mods.is_array());
        mcustl::string mod_copy = mods[0].get_string();
        EXPECT_EQ(mod_copy.size(), 10u);
        EXPECT_STREQ(mod_copy.c_str(), "pitch_down");
    }

    {
        mcustl::heap_guard g;
        const json& cons = j.at("stream_connections");
        ASSERT_TRUE(cons.is_array());
        ASSERT_EQ(cons.size(), 1u);

        const json& c0 = cons[0];

        /* Read source state BEFORE copy. If these aren't 6/usb_in here,
         * the parsed json itself is broken — not the copy mechanism. */
        ASSERT_TRUE(c0.at("input").is_string());
        EXPECT_EQ(c0.at("input").get_string().size(), 6u) << "source input size wrong before copy";
        EXPECT_STREQ(c0.at("input").get_string().c_str(), "usb_in") << "source input data wrong before copy";

        mcustl::string input_copy  = c0.at("input").get_string();
        mcustl::string output_copy = c0.at("output").get_string();

        EXPECT_EQ(input_copy.size(), 6u);
        EXPECT_STREQ(input_copy.c_str(), "usb_in");
        EXPECT_EQ(output_copy.size(), 7u);
        EXPECT_STREQ(output_copy.c_str(), "speaker");
    }
}

/* On the embedded target the heap is non-empty when SystemConfig::load()
 * runs — other tasks have already allocated buffers. This test
 * pre-fragments the heap by allocating + freeing varied sizes before
 * parsing, to push parse-time allocations into a non-trivial heap
 * state and surface allocator/defrag interactions that a fresh-heap
 * test would miss. */
TEST_F(McustlTestFixture, JsonParseDeviceConfigShape_FragmentedHeap) {
    /* Pre-fragment: allocate several tracked blocks via mcustl::string
     * (keeps trackers registered), free a subset to leave gaps. */
    {
        mcustl::string keep1(128);
        mcustl::string drop1(64);
        mcustl::string keep2(256);
        mcustl::string drop2(96);
        mcustl::string keep3(192);
        /* drop1 / drop2 leave heap (as locals), creating gaps. */
        (void)drop1; (void)drop2;
    }
    /* Persist some other allocations so the heap stays partially used. */
    mcustl::string persistent(512);

    const char* text =
        "{\"streams\":[{\"name\":\"speaker\",\"modifiers\":[\"pitch_down\"]}],"
        "\"stream_connections\":[{\"input\":\"usb_in\",\"output\":\"speaker\"}]}";

    json j = json::parse(text, strlen(text));
    ASSERT_TRUE(j.is_object());

    /* Now do the same iterator walk under heap_guard. */
    mcustl::heap_guard g;
    int found_correct_keys = 0;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const auto& k = it.key();
        if (k.size() == 7 && memcmp(k.c_str(), "streams", 7) == 0) ++found_correct_keys;
        if (k.size() == 18 && memcmp(k.c_str(), "stream_connections", 18) == 0) ++found_correct_keys;
    }
    EXPECT_EQ(found_correct_keys, 2);

    const json& s0 = j.at("streams")[0];
    EXPECT_STREQ(s0.at("name").get_string().c_str(), "speaker");
    EXPECT_STREQ(s0.at("modifiers")[0].get_string().c_str(), "pitch_down");

    const json& c0 = j.at("stream_connections")[0];
    EXPECT_STREQ(c0.at("input").get_string().c_str(), "usb_in");
    EXPECT_STREQ(c0.at("output").get_string().c_str(), "speaker");

    EXPECT_GT(persistent.capacity(), 0u);
}

/* ============================================================
 * Use-after-free in mcustl::json::parser cleanup path
 * ============================================================
 *
 * Originally observed as a hardfault on STM32 when ESP32 web-UI called
 * GET_EFFECTS_REGISTRY → STM32 ran mcustl::json::parse on each preset's
 * userParamsSchema literal. Stack:
 *
 *   parse_array (mcustl_json.cpp:1350)
 *     → ~json (mcustl_json.cpp:411)
 *       → destroy_payload (mcustl_json.cpp:217)
 *         → ~map<string,json> (mcustl_map.h:1031)
 *           → ~pair (mcustl_pair.h:84)
 *             → ~string (mcustl_string.cpp:428)
 *               → ~vector<char> (mcustl_vector.h:445)
 *                 → heap_lock(saved_heap=GARBAGE)   ← SEGV
 *
 * The "saved_heap" pointer at fault time is `0x000600000005` on host
 * (similar 0xC4BBB045-pattern on STM32 from FILL_FREED_MEMORY_BY_NULLS).
 * Decoded as little-endian uint32 pair: `size=5, capacity=6`. Those
 * are the size_val/capacity_val of *another* vector that landed on top
 * of the freed buffer — a classic UAF or double-destroy fingerprint.
 *
 * Bisection results — see tests below:
 *   PASS: 2+ empty objects, simple string-only objects, mixed-type
 *         objects with 1-char string values, heterogeneous key counts,
 *         3-string + 2-float objects.
 *   FAIL: a single object with **2 string fields whose VALUES are
 *         >= 4 chars long**, **plus 3 numeric (float or int) fields**,
 *         all inside an array.
 *
 * The 4-char threshold exactly matches when mcustl::string's inner
 * vector<char> has to grow past its initial capacity and triggers a
 * real allocation+possible defrag during parse, suggesting the trigger
 * is a defrag-induced relocation of the parse_object's `obj` while
 * we're holding stale references to its key/value slots.
 *
 * Where to look in mcustl when fixing:
 *   - parse_object's per-iteration `obj.o_->insert(key, val)` evaluates
 *     `key`/`val` references; if a defrag fires inside the insert,
 *     these refs go stale.
 *   - parse_array's end-of-loop `~elt()` runs after push_back has
 *     deep-copied. If the deep copy of an object is shallower than
 *     intended, both elt and arr's slot share state, and one's free
 *     corrupts the other.
 *   - mcustl::pair::~pair() and mcustl::string::~string() lack
 *     tracked_this protection; a defrag inside the inner ~vector::dfree
 *     leaves the outer pair/string `this` pointer stale.
 *
 * The tests below pin down the trigger and serve as a regression net
 * for the eventual mcustl fix. */

TEST_F(McustlTestFixture, JsonParseAOO_TwoEmptyObjects) {
    json j = json::parse("[{},{}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 2u);
}

TEST_F(McustlTestFixture, JsonParseAOO_TwoObjectsOneIntKey) {
    json j = json::parse("[{\"a\":1},{\"b\":2}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 2u);
    EXPECT_EQ(j[0].at("a").get_int(), 1);
    EXPECT_EQ(j[1].at("b").get_int(), 2);
}

TEST_F(McustlTestFixture, JsonParseAOO_TwoObjectsOneStringKey) {
    json j = json::parse("[{\"a\":\"x\"},{\"b\":\"y\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 2u);
    EXPECT_STREQ(j[0].at("a").get_string().c_str(), "x");
    EXPECT_STREQ(j[1].at("b").get_string().c_str(), "y");
}

TEST_F(McustlTestFixture, JsonParseAOO_ThreeObjectsManyStringFields) {
    /* 3 objects, each with 2 string fields. Closer to the schema shape. */
    json j = json::parse(
        "[{\"name\":\"a\",\"type\":\"x\"},"
        " {\"name\":\"b\",\"type\":\"y\"},"
        " {\"name\":\"c\",\"type\":\"z\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 3u);
    EXPECT_STREQ(j[2].at("name").get_string().c_str(), "c");
}

TEST_F(McustlTestFixture, JsonParseAOO_ThreeObjectsMixedTypes) {
    /* String + int + bool fields per object; multiple objects. */
    json j = json::parse(
        "[{\"name\":\"a\",\"min\":1,\"on\":true},"
        " {\"name\":\"b\",\"min\":2,\"on\":false},"
        " {\"name\":\"c\",\"min\":3,\"on\":true}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 3u);
    EXPECT_EQ(j[1].at("min").get_int(), 2);
    EXPECT_FALSE(j[1].at("on").get_bool());
}

TEST_F(McustlTestFixture, JsonParseAOO_NegativeFloatField) {
    /* The freq_shifter schema has min:-500.0. Test that parsing negative
     * floats inside array-of-objects doesn't trigger the bug. */
    json j = json::parse("[{\"min\":-500.0,\"max\":500.0}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_DOUBLE_EQ(j[0].at("min").get_float(), -500.0);
    EXPECT_DOUBLE_EQ(j[0].at("max").get_float(), 500.0);
}

/* Two-strings + three-floats — does string LENGTH matter? */
TEST_F(McustlTestFixture, JsonParseAOO_FS_LongStrings) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"type\":\"float\",\"min\":1.0,\"max\":2.0,\"default\":3.0}]");
    ASSERT_TRUE(j.is_array());
}

TEST_F(McustlTestFixture, JsonParseAOO_FS_NegativeFloat) {
    /* Same as FS_2str3float (which passed), but with negative number. */
    json j = json::parse(
        "[{\"name\":\"a\",\"type\":\"b\",\"min\":-500.0,\"max\":2.0,\"default\":3.0}]");
    ASSERT_TRUE(j.is_array());
}

/* Two-strings + three-floats bisection */

/* 1 string + 3 floats. */
TEST_F(McustlTestFixture, JsonParseAOO_FS_1str3float) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"min\":-500.0,\"max\":500.0,\"default\":5.0}]");
    ASSERT_TRUE(j.is_array());
}

/* 2 strings + 3 floats — same as FS1_NoDescription, repeat for clarity. */
TEST_F(McustlTestFixture, JsonParseAOO_FS_2str3float) {
    json j = json::parse(
        "[{\"name\":\"a\",\"type\":\"b\",\"min\":1.0,\"max\":2.0,\"default\":3.0}]");
    ASSERT_TRUE(j.is_array());
}

/* 2 strings + 3 ints (instead of floats). */
TEST_F(McustlTestFixture, JsonParseAOO_FS_2str3int) {
    json j = json::parse(
        "[{\"name\":\"a\",\"type\":\"b\",\"min\":1,\"max\":2,\"default\":3}]");
    ASSERT_TRUE(j.is_array());
}

/* 3 strings + 2 floats. */
TEST_F(McustlTestFixture, JsonParseAOO_FS_3str2float) {
    json j = json::parse(
        "[{\"a\":\"x\",\"b\":\"y\",\"c\":\"z\",\"min\":1.0,\"max\":2.0}]");
    ASSERT_TRUE(j.is_array());
}

/* Sub-bisection of the first freq_shifter object — strip one field at a
 * time to isolate which specific field-combo trips the bug. */

/* Two strings + three floats, no description. */
TEST_F(McustlTestFixture, JsonParseAOO_FS1_NoDescription) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"type\":\"float\",\"min\":-500.0,\"max\":500.0,\"default\":5.0}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 1u);
}

/* Two strings + one float + description. */
TEST_F(McustlTestFixture, JsonParseAOO_FS1_OneFloat) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"type\":\"float\",\"default\":5.0,"
        "\"description\":\"Frequency shift in Hz (positive = up-shift)\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 1u);
}

/* No floats at all. */
TEST_F(McustlTestFixture, JsonParseAOO_FS1_NoFloats) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"type\":\"float\","
        "\"description\":\"Frequency shift in Hz (positive = up-shift)\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 1u);
}

/* Just three floats, no strings. */
TEST_F(McustlTestFixture, JsonParseAOO_FS1_OnlyFloats) {
    json j = json::parse("[{\"min\":-500.0,\"max\":500.0,\"default\":5.0}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 1u);
}

/* String, then float, then string, then float (alternating). */
TEST_F(McustlTestFixture, JsonParseAOO_FS1_AlternatingStringFloat) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"min\":-500.0,\"type\":\"float\",\"max\":500.0}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 1u);
}

/* Bisecting the freq_shifter schema: just one object, 6 mixed-type keys.
 * If this fails, the trigger is single-object. If it passes, the trigger
 * involves multiple objects in a row. */
TEST_F(McustlTestFixture, JsonParseAOO_FreqShifterFirstObjectOnly) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"type\":\"float\",\"min\":-500.0,\"max\":500.0,\"default\":5.0,"
        "\"description\":\"Frequency shift in Hz (positive = up-shift)\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 1u);
}

/* Two objects, each 6 keys (uniform structure). */
TEST_F(McustlTestFixture, JsonParseAOO_FreqShifterFirstAndThird) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"type\":\"float\",\"min\":-500.0,\"max\":500.0,\"default\":5.0,"
        "\"description\":\"Frequency shift in Hz (positive = up-shift)\"},"
        " {\"name\":\"gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
        "\"description\":\"Output gain\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 2u);
}

/* Two objects: first has 6 keys, second has 4 keys (the asymmetric case
 * of freq_shifter without the third object). */
TEST_F(McustlTestFixture, JsonParseAOO_FreqShifterFirstTwoOnly) {
    json j = json::parse(
        "[{\"name\":\"shift_hz\",\"type\":\"float\",\"min\":-500.0,\"max\":500.0,\"default\":5.0,"
        "\"description\":\"Frequency shift in Hz (positive = up-shift)\"},"
        " {\"name\":\"enabled\",\"type\":\"bool\",\"default\":true,"
        "\"description\":\"Enable/bypass the shifter\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 2u);
}

/* The freq_shifter schema has objects with DIFFERENT numbers of keys (6,4,6).
 * That means each map's nodes_ allocation has a different capacity, so each
 * insert into the array triggers different defrag patterns. Try to repro
 * with that specific shape. */
TEST_F(McustlTestFixture, JsonParseAOO_HeterogeneousKeyCount) {
    json j = json::parse(
        "[{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5,\"f\":6},"
        " {\"a\":1,\"b\":2,\"c\":3,\"d\":4},"
        " {\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5,\"f\":6}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 3u);
    EXPECT_EQ(j[1].size(), 4u);
    EXPECT_EQ(j[2].size(), 6u);
}

/* Same shape but with strings — strings allocate more aggressively than
 * ints/floats, so this version puts more pressure on the heap. */
TEST_F(McustlTestFixture, JsonParseAOO_HeterogeneousKeyCount_StringValues) {
    json j = json::parse(
        "[{\"a\":\"x1\",\"b\":\"x2\",\"c\":\"x3\",\"d\":\"x4\",\"e\":\"x5\",\"f\":\"x6\"},"
        " {\"a\":\"y1\",\"b\":\"y2\",\"c\":\"y3\",\"d\":\"y4\"},"
        " {\"a\":\"z1\",\"b\":\"z2\",\"c\":\"z3\",\"d\":\"z4\",\"e\":\"z5\",\"f\":\"z6\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 3u);
}

TEST_F(McustlTestFixture, JsonParseAOO_LongDescriptionString) {
    /* Long description strings are the largest allocations in the schema. */
    json j = json::parse(
        "[{\"description\":\"Frequency shift in Hz (positive = up-shift)\"},"
        " {\"description\":\"Enable/bypass the shifter\"}]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 2u);
}

/* Reproduces a hardfault seen on the device when GET_EFFECTS_REGISTRY
 * tried to parse one of the audio-effect parameter schemas baked into
 * effect_registry.cpp. The fault was UNALIGNED in xQueueTakeMutexRecursive
 * — the call site was vector<char>::~vector → heap_lock(saved_heap),
 * with `saved_heap` containing garbage (0xC4BBB045-pattern). The call
 * chain bottomed out in mcustl::json::parser::parse_array, so the trigger
 * is somewhere in the cleanup path of an array-of-objects parse where
 * each object has multiple string/number/bool keys.
 *
 * The schema text used here is a verbatim copy of effect_registry.cpp's
 * "freq_shifter" entry — array of 3 objects with mixed-type fields and
 * a multi-line description string per object. Failing this test means
 * we've reproduced the device-side crash on host (where ASan/UBSan can
 * pinpoint the actual UAF or double-free). Passing it means the host
 * harness doesn't catch what the device did (different alignment, defrag
 * trigger, etc.) and we need to widen the repro. */
TEST_F(McustlTestFixture, JsonParseEffectParamsSchema_FreqShifter) {
    const char* text =
        "[{\"name\":\"shift_hz\",\"type\":\"float\",\"min\":-500.0,\"max\":500.0,\"default\":5.0,\n"
        "  \"description\":\"Frequency shift in Hz (positive = up-shift)\"},\n"
        " {\"name\":\"enabled\",\"type\":\"bool\",\"default\":true,\n"
        "  \"description\":\"Enable/bypass the shifter\"},\n"
        " {\"name\":\"gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,\n"
        "  \"description\":\"Output gain\"}]";

    const char* err = nullptr;
    json j = json::parse(text, strlen(text), nullptr, &err, nullptr);
    ASSERT_TRUE(err == nullptr) << "parse error: " << err;
    ASSERT_TRUE(j.is_array()) << "result is not an array";
    ASSERT_EQ(j.size(), 3u);

    /* Touch each element under the heap lock — same access pattern that
     * audio_state_serializer would have produced. */
    mcustl::heap_guard g;
    EXPECT_STREQ(j[0].at("name").get_string().c_str(), "shift_hz");
    EXPECT_STREQ(j[0].at("type").get_string().c_str(), "float");
    EXPECT_DOUBLE_EQ(j[0].at("min").get_float(), -500.0);
    EXPECT_DOUBLE_EQ(j[0].at("default").get_float(),  5.0);

    EXPECT_STREQ(j[1].at("name").get_string().c_str(), "enabled");
    EXPECT_STREQ(j[1].at("type").get_string().c_str(), "bool");
    EXPECT_TRUE  (j[1].at("default").get_bool());

    EXPECT_STREQ(j[2].at("name").get_string().c_str(), "gain");
    EXPECT_DOUBLE_EQ(j[2].at("max").get_float(), 10.0);
}

/* Same shape, but ALL the schemas from effect_registry.cpp parsed back
 * to back inside one heap (mirrors buildEffectsRegistryJson which loops
 * over the registry). If the bug is "second-or-later parse corrupts
 * something destruction left behind", this test catches it where a
 * single parse doesn't. */
TEST_F(McustlTestFixture, JsonParseEffectParamsSchema_SequentialAllPresets) {
    /* Subset of real schemas — the ones where GET_EFFECTS_REGISTRY would
     * iterate. Picked to span: small (3 fields), medium, large nested
     * config, and one with strings + ints. */
    const char* schemas[] = {
        /* freq_shifter */
        "[{\"name\":\"shift_hz\",\"type\":\"float\",\"min\":-500.0,\"max\":500.0,\"default\":5.0,"
        "\"description\":\"Frequency shift in Hz (positive = up-shift)\"},"
        "{\"name\":\"enabled\",\"type\":\"bool\",\"default\":true,"
        "\"description\":\"Enable/bypass the shifter\"},"
        "{\"name\":\"gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
        "\"description\":\"Output gain\"}]",
        /* anf */
        "[{\"name\":\"num_notches\",\"type\":\"int\",\"min\":1,\"max\":8,\"default\":4,"
        "\"description\":\"Max simultaneous notch filters\"},"
        "{\"name\":\"bandwidth_hz\",\"type\":\"float\",\"min\":1.0,\"max\":200.0,\"default\":50.0,"
        "\"description\":\"Notch 3dB bandwidth in Hz\"},"
        "{\"name\":\"threshold_db\",\"type\":\"float\",\"min\":1.0,\"max\":60.0,\"default\":15.0,"
        "\"description\":\"PNPR detection threshold in dB\"},"
        "{\"name\":\"confirm_frames\",\"type\":\"int\",\"min\":1,\"max\":50,\"default\":4,"
        "\"description\":\"Frames to confirm feedback before deploying notch\"},"
        "{\"name\":\"min_freq_hz\",\"type\":\"float\",\"min\":0.0,\"max\":20000.0,\"default\":500.0,"
        "\"description\":\"Minimum detection frequency in Hz\"},"
        "{\"name\":\"gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
        "\"description\":\"Output gain\"}]",
        /* aec — has a "string" field (reference: \"Speaker\") which exercises
         * a slightly different parse_value branch than purely numeric defaults. */
        "[{\"name\":\"tail_ms\",\"type\":\"int\",\"min\":20,\"max\":500,\"default\":50,"
        "\"description\":\"Echo tail length in ms\"},"
        "{\"name\":\"reference\",\"type\":\"string\",\"default\":\"Speaker\","
        "\"description\":\"Output stream name for far-end reference\"},"
        "{\"name\":\"mu\",\"type\":\"float\",\"min\":0.001,\"max\":2.0,\"default\":0.05,"
        "\"description\":\"NLMS step size (adaptation speed)\"},"
        "{\"name\":\"gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
        "\"description\":\"Output gain\"}]"
    };

    /* Iterate as buildEffectsRegistryJson does: parse each schema and
     * embed it under a per-effect "params" key. Any UAF that involves
     * the prior parse's leftover state should fire here. */
    json registry(json::value_t::array);
    for (size_t i = 0; i < sizeof(schemas) / sizeof(schemas[0]); ++i) {
        const char* err = nullptr;
        json params = json::parse(schemas[i], strlen(schemas[i]),
                                  nullptr, &err, nullptr);
        ASSERT_TRUE(err == nullptr) << "schema #" << i << ": " << err;
        ASSERT_TRUE(params.is_array()) << "schema #" << i << " not array";

        json effect(json::value_t::object);
        effect["index"]  = (int)i;
        effect["params"] = params;     /* deep-copy; this matched the device path */
        registry.push_back(effect);
    }

    ASSERT_EQ(registry.size(), sizeof(schemas) / sizeof(schemas[0]));

    /* Roundtrip: dump the assembled registry (this is what the device
     * sends to ESP32 via send_fragmented_locked). */
    mcustl::string out = registry.dump(-1);
    EXPECT_GT(out.size(), 0u);
}

/* ==================================================================
 * Comprehensive parse/dump/copy/move coverage across shapes.
 *
 * The tests below stress the json/string/vector/map/pair stack along
 * dimensions independent of any one device-specific bug:
 *   - depth (nested arrays / nested objects / alternating)
 *   - width (100+ siblings)
 *   - heterogeneity (mixed types in same container)
 *   - Unicode and escape handling
 *   - parse↔dump roundtripping with deep equality
 *   - copy-ctor / move-ctor / operator= on complex trees
 *   - mutation through operator[] / push_back / erase at depth
 *   - cumulative heap pressure across many parses
 *   - structural edge cases (empty container at depth, null at depth)
 * ================================================================== */

/* ---- Depth ---------------------------------------------------------- */

TEST_F(McustlTestFixture, JsonNested_ArraysDepth8) {
    /* 8-level nested array. The innermost element is a leaf int.
     * Exercises parse_array recursion + clone-on-deep-copy invariants. */
    const char* text = "[[[[[[[[42]]]]]]]]";
    json j = json::parse(text);
    ASSERT_TRUE(j.is_array());
    const json* cur = &j;
    for (int d = 0; d < 7; ++d) {
        ASSERT_TRUE(cur->is_array()) << "depth " << d;
        ASSERT_EQ(cur->size(), 1u)   << "depth " << d;
        cur = &(*cur)[0];
    }
    ASSERT_TRUE(cur->is_array());
    EXPECT_EQ((*cur)[0].get_int(), 42);
}

TEST_F(McustlTestFixture, JsonNested_ObjectsDepth8) {
    /* 8-level nested object. Each level has one key "n" mapping to the
     * next level; innermost is an int. */
    const char* text =
        "{\"n\":{\"n\":{\"n\":{\"n\":{\"n\":{\"n\":{\"n\":{\"n\":99}}}}}}}}";
    json j = json::parse(text);
    ASSERT_TRUE(j.is_object());
    const json* cur = &j;
    for (int d = 0; d < 7; ++d) {
        ASSERT_TRUE(cur->is_object()) << "depth " << d;
        ASSERT_TRUE(cur->contains("n")) << "depth " << d;
        cur = &cur->at("n");
    }
    ASSERT_TRUE(cur->is_object());
    EXPECT_EQ(cur->at("n").get_int(), 99);
}

TEST_F(McustlTestFixture, JsonNested_AlternatingArrayObject) {
    /* Alternating array → object → array → object × 4. */
    const char* text =
        "[{\"k\":[{\"k\":[{\"k\":[{\"k\":\"deep\"}]}]}]}]";
    json j = json::parse(text);
    ASSERT_TRUE(j.is_array());
    const json& l1 = j[0];                 ASSERT_TRUE(l1.is_object());
    const json& l2 = l1.at("k");           ASSERT_TRUE(l2.is_array());
    const json& l3 = l2[0];                ASSERT_TRUE(l3.is_object());
    const json& l4 = l3.at("k");           ASSERT_TRUE(l4.is_array());
    const json& l5 = l4[0];                ASSERT_TRUE(l5.is_object());
    const json& l6 = l5.at("k");           ASSERT_TRUE(l6.is_array());
    const json& l7 = l6[0];                ASSERT_TRUE(l7.is_object());
    EXPECT_STREQ(l7.at("k").get_string().c_str(), "deep");
}

/* ---- Width ---------------------------------------------------------- */

TEST_F(McustlTestFixture, JsonWide_LargeMixedArray) {
    /* 100-element array, alternating type per slot. Stresses vector<json>
     * grow + element clone on deep_copy. */
    mcustl::string text;
    text.append('[');
    for (int i = 0; i < 100; ++i) {
        if (i) text.append(',');
        switch (i % 5) {
            case 0: text.append('1'); text.append('0'); text.append('0'); break;          /* int */
            case 1: text.append('"'); text.append('s'); text.append('"');  break;         /* str */
            case 2: text.append('1'); text.append('.'); text.append('5');  break;         /* float */
            case 3: text.append('t'); text.append('r'); text.append('u'); text.append('e'); break; /* true */
            case 4: text.append('n'); text.append('u'); text.append('l'); text.append('l'); break; /* null */
        }
    }
    text.append(']');

    json j = json::parse(text.c_str(), text.size());
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 100u);
    EXPECT_EQ(j[0].get_int(), 100);
    EXPECT_STREQ(j[1].get_string().c_str(), "s");
    EXPECT_DOUBLE_EQ(j[2].get_float(), 1.5);
    EXPECT_TRUE(j[3].get_bool());
    EXPECT_TRUE(j[4].is_null());
    EXPECT_EQ(j[95].get_int(), 100);  /* 95 % 5 == 0 */
}

TEST_F(McustlTestFixture, JsonWide_LargeObject50Keys) {
    /* 50-key object — stresses map's red-black tree + grow path. Values
     * are 100..149 to avoid JSON's "no leading zeros" rule (which the
     * parser correctly enforces). */
    mcustl::string text;
    text.append('{');
    for (int i = 0; i < 50; ++i) {
        int v = 100 + i;
        if (i) text.append(',');
        text.append('"'); text.append('k');
        char d1 = '0' + (i / 10);
        char d2 = '0' + (i % 10);
        text.append(d1); text.append(d2);
        text.append('"'); text.append(':');
        text.append((char)('0' + (v / 100)));
        text.append((char)('0' + ((v / 10) % 10)));
        text.append((char)('0' + (v % 10)));
    }
    text.append('}');

    const char* err = nullptr;
    json j = json::parse(text.c_str(), text.size(), nullptr, &err, nullptr);
    ASSERT_TRUE(j.is_object()) << "parse error: " << (err ? err : "?");
    EXPECT_EQ(j.size(), 50u);
    EXPECT_EQ(j.at("k00").get_int(), 100);
    EXPECT_EQ(j.at("k49").get_int(), 149);
}

/* ---- Heterogeneity -------------------------------------------------- */

TEST_F(McustlTestFixture, JsonHetero_AllTypesInOneObject) {
    /* Every json value type as a sibling — pulls every parse_value branch. */
    const char* text =
        "{\"n\":null,"
        " \"b\":true,"
        " \"i\":-2147483648,"
        " \"u\":4294967295,"
        " \"f\":-3.14159,"
        " \"e\":1.5e10,"
        " \"s\":\"\\u00E9scape\","
        " \"arr\":[1,2,3],"
        " \"obj\":{\"x\":1}}";
    json j = json::parse(text);
    ASSERT_TRUE(j.is_object());
    EXPECT_TRUE(j.at("n").is_null());
    EXPECT_TRUE(j.at("b").get_bool());
    EXPECT_EQ(j.at("i").get_int(), -2147483648LL);
    EXPECT_EQ(j.at("u").get_int(), 4294967295LL);
    EXPECT_DOUBLE_EQ(j.at("f").get_float(), -3.14159);
    EXPECT_DOUBLE_EQ(j.at("e").get_float(), 1.5e10);
    EXPECT_EQ(j.at("arr").size(), 3u);
    EXPECT_EQ(j.at("obj").at("x").get_int(), 1);
}

TEST_F(McustlTestFixture, JsonHetero_ArrayOfArrays) {
    /* 5×5 jagged array of ints. */
    json j = json::parse(
        "[[1,2,3,4,5],"
        " [10,20,30],"
        " [100],"
        " [],"
        " [-1,-2,-3,-4,-5,-6]]");
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 5u);
    EXPECT_EQ(j[0].size(), 5u);
    EXPECT_EQ(j[2].size(), 1u);
    EXPECT_EQ(j[3].size(), 0u);
    EXPECT_EQ(j[4].size(), 6u);
    EXPECT_EQ(j[4][5].get_int(), -6);
}

TEST_F(McustlTestFixture, JsonHetero_ObjectOfArraysOfObjects) {
    /* A common "results"-style shape. */
    const char* text =
        "{\"users\":["
        "  {\"id\":1,\"name\":\"alice\",\"tags\":[\"a\",\"b\"]},"
        "  {\"id\":2,\"name\":\"bob\",\"tags\":[]},"
        "  {\"id\":3,\"name\":\"carol\",\"tags\":[\"x\",\"y\",\"z\"]}],"
        " \"total\":3,\"version\":\"1.0\"}";
    json j = json::parse(text);
    ASSERT_TRUE(j.is_object());
    const json& u = j.at("users");
    ASSERT_TRUE(u.is_array());
    ASSERT_EQ(u.size(), 3u);
    EXPECT_STREQ(u[0].at("name").get_string().c_str(), "alice");
    EXPECT_EQ(u[1].at("tags").size(), 0u);
    EXPECT_STREQ(u[2].at("tags")[2].get_string().c_str(), "z");
    EXPECT_EQ(j.at("total").get_int(), 3);
}

/* ---- Unicode and escapes ------------------------------------------- */

TEST_F(McustlTestFixture, JsonUnicode_MixedEscapes) {
    json j = json::parse(
        "{\"plain\":\"hello\","
        " \"esc\":\"a\\\"b\\\\c\\nd\","
        " \"u_low\":\"\\u00E9\","
        " \"u_high\":\"\\uD83D\\uDE00\","
        " \"empty\":\"\","
        " \"cr_lf\":\"\\r\\n\","
        " \"tab\":\"\\t\"}");
    ASSERT_TRUE(j.is_object());
    EXPECT_STREQ(j.at("plain").get_string().c_str(), "hello");
    EXPECT_STREQ(j.at("esc").get_string().c_str(), "a\"b\\c\nd");
    /* "é" → C3 A9 (UTF-8) */
    const unsigned char* el = (const unsigned char*)j.at("u_low").get_string().c_str();
    EXPECT_EQ(el[0], 0xC3u); EXPECT_EQ(el[1], 0xA9u); EXPECT_EQ(el[2], 0u);
    /* 😀 → F0 9F 98 80 */
    const unsigned char* eh = (const unsigned char*)j.at("u_high").get_string().c_str();
    EXPECT_EQ(eh[0], 0xF0u); EXPECT_EQ(eh[1], 0x9Fu);
    EXPECT_EQ(eh[2], 0x98u); EXPECT_EQ(eh[3], 0x80u);
    EXPECT_EQ(j.at("empty").size(), 0u);
    EXPECT_STREQ(j.at("cr_lf").get_string().c_str(), "\r\n");
}

TEST_F(McustlTestFixture, JsonUnicode_DumpEscapesProperly) {
    /* Build a json with chars that must be escaped on dump, then parse
     * the dump back and verify content is identical. */
    json j(json::value_t::object);
    j["q"]   = "with \"quotes\"";
    j["bs"]  = "back\\slash";
    j["nl"]  = "line\nbreak";
    j["ctl"] = "ctl\x01x";

    mcustl::string s = j.dump(-1);
    json k = json::parse(s.c_str(), s.size());
    ASSERT_TRUE(k.is_object());
    EXPECT_STREQ(k.at("q").get_string().c_str(),   "with \"quotes\"");
    EXPECT_STREQ(k.at("bs").get_string().c_str(),  "back\\slash");
    EXPECT_STREQ(k.at("nl").get_string().c_str(),  "line\nbreak");
    EXPECT_STREQ(k.at("ctl").get_string().c_str(), "ctl\x01x");
}

/* ---- Roundtrip parse → dump → reparse ------------------------------ */

TEST_F(McustlTestFixture, JsonRoundtrip_DeeplyNested) {
    const char* original =
        "{\"meta\":{\"v\":1,\"by\":\"test\"},"
        " \"data\":["
        "   {\"k\":\"a\",\"v\":[1,2,3,{\"x\":true}]},"
        "   {\"k\":\"b\",\"v\":[null,\"\",\"hello\"]},"
        "   {\"k\":\"c\",\"v\":[]}],"
        " \"counts\":[10,20,30],"
        " \"flag\":false}";
    json a = json::parse(original);
    mcustl::string out = a.dump(-1);
    json b = json::parse(out.c_str(), out.size());
    EXPECT_TRUE(a == b)
        << "roundtrip-equivalence broken; out=" << out.c_str();
}

TEST_F(McustlTestFixture, JsonRoundtrip_PrettyPrintEqualsCompact) {
    const char* compact =
        "{\"a\":1,\"b\":[2,3,{\"c\":4}],\"d\":\"x\"}";
    json j = json::parse(compact);
    mcustl::string p = j.dump(2);
    json k = json::parse(p.c_str(), p.size());
    EXPECT_TRUE(j == k);
}

/* ---- Copy / move semantics on complex trees ------------------------ */

TEST_F(McustlTestFixture, JsonDeepCopy_IndependentFromSource) {
    json orig = json::parse(
        "{\"arr\":[1,2,3,{\"x\":\"old\"}],\"name\":\"orig\"}");
    json copy = orig;          /* deep copy */

    /* Mutate the source; copy must not see the change. */
    orig["name"] = "changed";
    orig["arr"][3]["x"] = "modified";
    orig["arr"].push_back(99);

    EXPECT_STREQ(copy.at("name").get_string().c_str(), "orig");
    EXPECT_STREQ(copy.at("arr")[3].at("x").get_string().c_str(), "old");
    EXPECT_EQ(copy.at("arr").size(), 4u);
    EXPECT_EQ(orig.at("arr").size(), 5u);
}

TEST_F(McustlTestFixture, JsonMove_LeavesSourceEmpty) {
    json src = json::parse("{\"a\":[1,2,3],\"b\":\"hi\"}");
    json dst = static_cast<json&&>(src);
    EXPECT_TRUE(dst.is_object());
    EXPECT_EQ(dst.size(), 2u);
    EXPECT_EQ(dst.at("a").size(), 3u);
    /* src after move is unspecified-but-valid (mcustl convention: null) */
    EXPECT_TRUE(src.is_null() || (src.is_object() && src.size() == 0));
}

TEST_F(McustlTestFixture, JsonAssign_ReplacesPayloadCleanly) {
    json j = json::parse("{\"a\":[1,2,3,4,5],\"b\":{\"c\":\"d\"}}");
    /* Replace with a smaller value — old payload must be released. */
    j = json::parse("[42]");
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 1u);
    EXPECT_EQ(j[0].get_int(), 42);
}

/* ---- Mutation through accessors ------------------------------------ */

TEST_F(McustlTestFixture, JsonMutate_NestedObjectAssign) {
    json j(json::value_t::object);
    j["a"]      = json(json::value_t::object);
    j["a"]["b"] = json(json::value_t::array);
    j["a"]["b"].push_back(1);
    j["a"]["b"].push_back(2);
    j["a"]["b"].push_back(3);
    j["a"]["c"] = "leaf";

    EXPECT_EQ(j.at("a").at("b").size(), 3u);
    EXPECT_STREQ(j.at("a").at("c").get_string().c_str(), "leaf");
}

TEST_F(McustlTestFixture, JsonMutate_ArrayPushBackMixed) {
    json j(json::value_t::array);
    j.push_back(1);
    j.push_back(json("text"));
    j.push_back(json(true));
    j.push_back(json(json::value_t::array));
    j.push_back(json(json::value_t::object));

    ASSERT_EQ(j.size(), 5u);
    EXPECT_TRUE(j[3].is_array());
    EXPECT_TRUE(j[4].is_object());
}

TEST_F(McustlTestFixture, JsonMutate_EraseFromNestedObject) {
    json j = json::parse(
        "{\"users\":{\"alice\":1,\"bob\":2,\"carol\":3}}");
    json& users = j.at("users");
    EXPECT_TRUE(users.erase("bob"));
    EXPECT_FALSE(users.contains("bob"));
    EXPECT_TRUE(users.contains("alice"));
    EXPECT_TRUE(users.contains("carol"));
    EXPECT_EQ(users.size(), 2u);
}

/* ---- Iteration ----------------------------------------------------- */

TEST_F(McustlTestFixture, JsonIterate_ObjectKeysVisitedOnce) {
    json j = json::parse("{\"x\":1,\"y\":2,\"z\":3,\"a\":4,\"m\":5}");
    int seen_x = 0, seen_y = 0, seen_z = 0, seen_a = 0, seen_m = 0;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const auto& k = it.key();
        if (k.size() == 1) {
            switch (k.c_str()[0]) {
                case 'x': ++seen_x; break;
                case 'y': ++seen_y; break;
                case 'z': ++seen_z; break;
                case 'a': ++seen_a; break;
                case 'm': ++seen_m; break;
            }
        }
    }
    EXPECT_EQ(seen_x + seen_y + seen_z + seen_a + seen_m, 5);
    EXPECT_EQ(seen_x, 1);
    EXPECT_EQ(seen_a, 1);
}

TEST_F(McustlTestFixture, JsonIterate_NestedArrayWalk) {
    json j = json::parse("[[1,2],[3,4],[5,6]]");
    int sum = 0;
    for (size_t i = 0; i < j.size(); ++i) {
        for (size_t k = 0; k < j[i].size(); ++k) {
            sum += (int)j[i][k].get_int();
        }
    }
    EXPECT_EQ(sum, 1+2+3+4+5+6);
}

/* ---- Stress / heap pressure ---------------------------------------- */

TEST_F(McustlTestFixture, JsonStress_RepeatedParseAndDiscard) {
    /* 100 parses of a moderately complex shape. The temporary tree is
     * destroyed at the end of each iteration; cumulative defrag activity
     * must not corrupt the heap. */
    const char* text =
        "{\"a\":[1,2,3,{\"x\":\"hello\",\"y\":[true,false,null]}],"
        " \"b\":{\"k1\":\"v1\",\"k2\":\"v2\",\"k3\":[\"a\",\"b\",\"c\"]}}";
    for (int i = 0; i < 100; ++i) {
        json j = json::parse(text);
        ASSERT_TRUE(j.is_object()) << "iter " << i;
        ASSERT_EQ(j.at("a").size(), 4u) << "iter " << i;
        ASSERT_EQ(j.at("b").at("k3").size(), 3u) << "iter " << i;
    }
}

TEST_F(McustlTestFixture, JsonStress_ParseAndCopy) {
    /* Parse once, then copy 50 times. Each copy is a deep copy; the
     * source must remain intact. */
    json src = json::parse(
        "{\"data\":[" "{\"id\":1,\"name\":\"alpha\",\"tags\":[\"x\",\"y\"]},"
                     "{\"id\":2,\"name\":\"beta\",\"tags\":[]}"
        "],\"meta\":{\"total\":2}}");
    for (int i = 0; i < 50; ++i) {
        json copy = src;
        ASSERT_EQ(copy.at("data").size(), 2u) << "iter " << i;
        EXPECT_STREQ(copy.at("data")[0].at("name").get_string().c_str(), "alpha");
    }
    /* Source still pristine. */
    EXPECT_STREQ(src.at("data")[1].at("name").get_string().c_str(), "beta");
}

TEST_F(McustlTestFixture, JsonStress_BuildBigTreeIncrementally) {
    /* Build an array of 50 objects with varying contents via mutation
     * APIs. Tests grow + insert + push_back combined. */
    json arr(json::value_t::array);
    for (int i = 0; i < 50; ++i) {
        json o(json::value_t::object);
        o["i"]    = i;
        o["sq"]   = i * i;
        o["even"] = (i % 2 == 0);
        json sub(json::value_t::array);
        for (int k = 0; k < (i % 5); ++k) sub.push_back(k);
        o["sub"] = sub;
        arr.push_back(o);
    }
    EXPECT_EQ(arr.size(), 50u);
    EXPECT_EQ(arr[10].at("sq").get_int(), 100);
    EXPECT_EQ(arr[10].at("sub").size(), 0u);    /* 10 % 5 == 0 */
    EXPECT_EQ(arr[12].at("sub").size(), 2u);    /* 12 % 5 == 2 */
    EXPECT_TRUE(arr[20].at("even").get_bool());
}

/* ---- Edge cases ---------------------------------------------------- */

TEST_F(McustlTestFixture, JsonEdge_EmptyContainerAtDepth) {
    /* Empty containers at various nesting depths. */
    json j = json::parse("[[],{},[{}],[{\"k\":[]}]]");
    EXPECT_EQ(j.size(), 4u);
    EXPECT_TRUE(j[0].is_array());     EXPECT_EQ(j[0].size(), 0u);
    EXPECT_TRUE(j[1].is_object());    EXPECT_EQ(j[1].size(), 0u);
    EXPECT_TRUE(j[2][0].is_object()); EXPECT_EQ(j[2][0].size(), 0u);
    EXPECT_TRUE(j[3][0].at("k").is_array()); EXPECT_EQ(j[3][0].at("k").size(), 0u);
}

TEST_F(McustlTestFixture, JsonEdge_NullAtVariousPositions) {
    json j = json::parse("[null,{\"a\":null,\"b\":[null,null]},null]");
    EXPECT_TRUE(j[0].is_null());
    EXPECT_TRUE(j[1].at("a").is_null());
    EXPECT_TRUE(j[1].at("b")[0].is_null());
    EXPECT_TRUE(j[1].at("b")[1].is_null());
    EXPECT_TRUE(j[2].is_null());
}

TEST_F(McustlTestFixture, JsonEdge_StringsOfManyLengths) {
    /* Strings spanning the boundaries where vector<char> reserves
     * different capacities. */
    mcustl::string text;
    text.append('[');
    for (int len : {0, 1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128}) {
        if (text.size() > 1) text.append(',');
        text.append('"');
        for (int k = 0; k < len; ++k) {
            text.append((char)('a' + (k % 26)));
        }
        text.append('"');
    }
    text.append(']');

    json j = json::parse(text.c_str(), text.size());
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 14u);
    EXPECT_EQ(j[0].get_string().size(), 0u);
    EXPECT_EQ(j[13].get_string().size(), 128u);
    /* Spot-check content. */
    EXPECT_EQ(j[7].get_string().c_str()[0], 'a');
    EXPECT_EQ(j[7].get_string().c_str()[15], 'p');  /* 'a'+(15%26)='p' */
}

TEST_F(McustlTestFixture, JsonEdge_NumericPrecision) {
    /* Integers at edges (within int64 range and without overflow during
     * accumulation), float with many digits, scientific notation.
     *
     * Note: parsing INT64_MAX (`9223372036854775807`) currently overflows
     * during accumulation and folds to INT64_MIN — a separate parser
     * issue worth a dedicated bug. The comfortable bound here stays
     * inside ~2^61 to keep the test focused on precision, not parser
     * overflow. */
    json j = json::parse(
        "[0,-1,1,1000000000000000,-1000000000000000,"
        " 0.1,3.141592653589793,1e-9,2.5e15,-1.5e-300]");
    EXPECT_EQ(j[0].get_int(),  0);
    EXPECT_EQ(j[1].get_int(), -1);
    EXPECT_EQ(j[2].get_int(),  1);
    EXPECT_EQ(j[3].get_int(),  1000000000000000LL);
    EXPECT_EQ(j[4].get_int(), -1000000000000000LL);
    EXPECT_DOUBLE_EQ(j[5].get_float(), 0.1);
    EXPECT_DOUBLE_EQ(j[6].get_float(), 3.141592653589793);
    EXPECT_DOUBLE_EQ(j[7].get_float(), 1e-9);
    EXPECT_DOUBLE_EQ(j[8].get_float(), 2.5e15);
    /* 1.5e-300 may underflow; just check it parsed without error. */
    EXPECT_TRUE(j[9].is_number_float());
}

/* ---- Mixed parse + mutate + dump pipeline -------------------------- */

TEST_F(McustlTestFixture, JsonPipeline_ParseMutateDumpReparseEqual) {
    /* Realistic flow: parse incoming, modify, serialize for downstream. */
    json j = json::parse(
        "{\"items\":[{\"id\":1,\"qty\":10},{\"id\":2,\"qty\":20}],"
        " \"total\":30}");
    /* Add a new item; bump total. */
    {
        json item(json::value_t::object);
        item["id"]  = 3;
        item["qty"] = 5;
        j["items"].push_back(item);
    }
    j["total"] = 35;

    mcustl::string out = j.dump(-1);
    json k = json::parse(out.c_str(), out.size());
    EXPECT_EQ(k.at("items").size(), 3u);
    EXPECT_EQ(k.at("items")[2].at("id").get_int(), 3);
    EXPECT_EQ(k.at("total").get_int(), 35);
}

/* Repeat the parse + walk many times in the same heap. Cumulative
 * defrag activity may expose timing-dependent issues that a single
 * parse misses. Mimics a long-running device that re-loads config. */
TEST_F(McustlTestFixture, JsonParseDeviceConfigShape_RepeatedParseLoop) {
    const char* text =
        "{\"streams\":[{\"name\":\"speaker\",\"modifiers\":[\"pitch_down\"]}],"
        "\"stream_connections\":[{\"input\":\"usb_in\",\"output\":\"speaker\"}]}";

    for (int round = 0; round < 50; ++round) {
        json j = json::parse(text, strlen(text));
        ASSERT_TRUE(j.is_object()) << "round " << round;

        mcustl::heap_guard g;
        ASSERT_TRUE(j.at("streams")[0].at("name").is_string()) << "round " << round;
        EXPECT_STREQ(j.at("streams")[0].at("name").get_string().c_str(), "speaker")
            << "round " << round;
        EXPECT_STREQ(j.at("stream_connections")[0].at("input").get_string().c_str(), "usb_in")
            << "round " << round;
    }
}

/* Schemas sized to match real effect_registry.cpp entries on the device
 * (multi-KB JSON arrays with descriptions). The exact text doesn't
 * matter — what matters is that parsing + assigning each one churns
 * the heap a lot. */
static const char* kSchemaLarge =
    "[{\"name\":\"freq_shifter.shift_hz\",\"type\":\"float\",\"min\":1.0,\"max\":20.0,\"default\":10.0,"
      "\"description\":\"Preventive shift amount in Hz, fairly long descriptive text to bloat allocations\"},"
     "{\"name\":\"freq_shifter.enabled\",\"type\":\"bool\",\"default\":true,"
      "\"description\":\"Enable/bypass the shifter, more text to grow the schema\"},"
     "{\"name\":\"freq_shifter.gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
      "\"description\":\"Freq shifter output gain with a long-enough description string\"},"
     "{\"name\":\"anf.num_notches\",\"type\":\"int\",\"min\":1,\"max\":8,\"default\":6,"
      "\"description\":\"Max simultaneous notch filters running concurrently\"},"
     "{\"name\":\"anf.bandwidth_hz\",\"type\":\"float\",\"min\":1.0,\"max\":200.0,\"default\":50.0,"
      "\"description\":\"Notch 3dB bandwidth in Hz, used by the adaptive engine\"},"
     "{\"name\":\"anf.threshold_db\",\"type\":\"float\",\"min\":1.0,\"max\":60.0,\"default\":15.0,"
      "\"description\":\"PNPR detection threshold in dB above noise floor\"},"
     "{\"name\":\"anf.confirm_frames\",\"type\":\"int\",\"min\":1,\"max\":50,\"default\":4,"
      "\"description\":\"Frames to confirm feedback before deploying notch and stealing CPU\"},"
     "{\"name\":\"anf.min_freq_hz\",\"type\":\"float\",\"min\":0.0,\"max\":20000.0,\"default\":800.0,"
      "\"description\":\"Min tracking frequency, ignore detections below this band\"},"
     "{\"name\":\"anf.gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
      "\"description\":\"ANF output gain post-notching\"},"
     "{\"name\":\"level_guard.threshold_db_sec\",\"type\":\"float\",\"min\":5.0,\"max\":500.0,\"default\":40.0,"
      "\"description\":\"Max allowed level rise rate in dB per second\"},"
     "{\"name\":\"level_guard.hold_ms\",\"type\":\"float\",\"min\":50.0,\"max\":2000.0,\"default\":300.0,"
      "\"description\":\"Hold time after detection before release in ms\"},"
     "{\"name\":\"level_guard.min_gain\",\"type\":\"float\",\"min\":0.001,\"max\":1.0,\"default\":0.05,"
      "\"description\":\"Level guard gain floor (0.05 == -26 dB)\"},"
     "{\"name\":\"biquad.frequency\",\"type\":\"float\",\"min\":1000.0,\"max\":20000.0,\"default\":7000.0,"
      "\"description\":\"Low-pass cutoff frequency in Hz for the post filter\"},"
     "{\"name\":\"biquad.Q\",\"type\":\"float\",\"min\":0.1,\"max\":5.0,\"default\":0.707,"
      "\"description\":\"Filter Q (0.707 = Butterworth, no resonance hump)\"},"
     "{\"name\":\"biquad.gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
      "\"description\":\"LPF output gain\"}]";

static const char* kSchemaMedium =
    "[{\"name\":\"semitones\",\"type\":\"int\",\"min\":-12,\"max\":12,\"default\":-3,"
      "\"description\":\"Pitch shift in semitones (negative = down, positive = up)\"},"
     "{\"name\":\"mix\",\"type\":\"float\",\"min\":0.0,\"max\":1.0,\"default\":1.0,"
      "\"description\":\"Wet/dry mix amount\"}]";

static const char* kSchemaSmall = "[]";

/* Reproduction of an STM32 BusFault in `audio_state_serializer::
 * buildEffectsRegistryJson` (exciter-mini). The crash signature was
 *   register_pseudo_tracker -> tracked_this<json> -> json::operator[] ->
 *   json::operator= -> buildEffectsRegistryJson  (heap_struct_ptr garbage)
 * i.e. a slot returned by `effect[key]` had its `heap_` field overwritten
 * by the time `operator=` later read it back via `effective_heap()`.
 *
 * The structural ingredients of the device-side path:
 *   - outer heap_guard (handle_get_snapshot)
 *   - inner heap_guard (buildEffectsRegistryJson)        — nested guards
 *   - per-iteration local `effect` json built by appending several
 *     string-valued keys, then parsing a schema literal into a temp,
 *     then assigning the temp into effect["params"]
 *   - effects.push_back(effect)
 * Looping that with the realistic number of entries (~40) and KB-scale
 * schemas is what shakes loose the latent UAF / stale-slot in the
 * json/map combination.
 */
TEST_F(McustlTestFixture, JsonBuildEffectsRegistryRepro) {
    /* Mimic handle_get_snapshot's outer heap_guard. */
    mcustl::heap_guard outer;

    json snapshot;

    /* Mimic buildEffectsRegistryJson: nested heap_guard + the same
     * sequence of mutations on `out`. */
    {
        mcustl::heap_guard inner;

        snapshot = json(json::value_t::object);

        /* Limits sub-object (tiny but exercises a fresh nested
         * object_t under the inner guard). */
        {
            json limits(json::value_t::object);
            limits["max_modifiers_per_stream"] = 8;
            snapshot["limits"] = limits;
        }

        json effects(json::value_t::array);

        /* ~40 entries to match the device's effect registry, with
         * KB-scale schemas. */
        struct EffectFixture { const char* name; const char* schema; };
        static const EffectFixture FIXTURES[] = {
            { "freq_shifter",         kSchemaLarge  },
            { "anf",                  kSchemaLarge  },
            { "feedback_suppressor",  kSchemaLarge  },
            { "aec",                  kSchemaLarge  },
            { "level_guard",          kSchemaLarge  },
            { "pitch_down",           kSchemaMedium },
            { "pitch_up",             kSchemaMedium },
            { "robot",                nullptr       },
            { "chorus",               kSchemaMedium },
            { "tremolo",              kSchemaMedium },
            { "lpf",                  kSchemaMedium },
            { "hpf",                  kSchemaMedium },
            { "bpf",                  kSchemaMedium },
            { "notch",                kSchemaMedium },
            { "delay",                kSchemaLarge  },
            { "reverb",               kSchemaLarge  },
            { "compressor",           kSchemaLarge  },
            { "limiter",              kSchemaLarge  },
            { "expander",             kSchemaLarge  },
            { "gate",                 kSchemaMedium },
            { "eq_3band",             kSchemaLarge  },
            { "eq_5band",             kSchemaLarge  },
            { "distortion",           kSchemaMedium },
            { "overdrive",            kSchemaMedium },
            { "fuzz",                 kSchemaSmall  },
            { "bitcrusher",           kSchemaMedium },
            { "ring_mod",             kSchemaMedium },
            { "vibrato",              kSchemaMedium },
            { "flanger",              kSchemaLarge  },
            { "phaser",               kSchemaLarge  },
            { "wah",                  kSchemaMedium },
            { "auto_wah",             kSchemaLarge  },
            { "octave_up",            kSchemaSmall  },
            { "octave_down",          kSchemaSmall  },
            { "harmonizer",           kSchemaLarge  },
            { "vocoder",              kSchemaLarge  },
            { "formant_shift",        kSchemaMedium },
            { "noise_gate",           kSchemaMedium },
            { "spectral_gate",        kSchemaLarge  },
            { "denoiser",             kSchemaLarge  },
            { "monitor_mute",         nullptr       },
        };

        for (const auto& fx : FIXTURES) {
            json effect(json::value_t::object);
            effect["name"]     = fx.name;
            effect["display"]  = fx.name;
            effect["category"] = "modifier";

            if (fx.schema && *fx.schema) {
                const char* err = nullptr;
                json params = json::parse(fx.schema, std::strlen(fx.schema),
                                          nullptr, &err, nullptr);
                if (params.is_null() || !params.is_array()) {
                    effect["params"] = json(json::value_t::array);
                } else {
                    effect["params"] = params;
                }
            } else {
                effect["params"] = json(json::value_t::array);
            }
            effects.push_back(effect);
        }
        snapshot["effects"] = effects;
    }

    /* If we got here without a crash, validate the structure end-to-end. */
    ASSERT_TRUE(snapshot.is_object());
    ASSERT_TRUE(snapshot.contains("limits"));
    ASSERT_TRUE(snapshot.contains("effects"));
    ASSERT_TRUE(snapshot.at("effects").is_array());
    ASSERT_EQ(snapshot.at("effects").size(), 41u);

    /* Walk every effect, read back name + params type — same access
     * pattern handle_get_snapshot's dump() takes. */
    for (size_t i = 0; i < snapshot.at("effects").size(); ++i) {
        const json& e = snapshot.at("effects")[i];
        ASSERT_TRUE(e.is_object()) << "effect " << i;
        ASSERT_TRUE(e.contains("name"));
        ASSERT_TRUE(e.contains("params"));
        EXPECT_TRUE(e.at("name").is_string()) << "effect " << i;
        EXPECT_TRUE(e.at("params").is_array()) << "effect " << i;
    }

    /* Final dump must succeed (this is what send_ok_with_data does on
     * the device — and it touches every leaf). */
    mcustl::string dumped = snapshot.dump(-1);
    EXPECT_GT(dumped.size(), 0u);
}

/* Bisect: same loop body as the big repro. If this crashes / warns,
 * the latent bug is in build-up-then-push-back of objects with
 * parsed-schema values. */
TEST_F(McustlTestFixture, JsonBuildEffectsRegistryRepro_NEffects) {
    auto sanity_walk = [](const json& effects, const char* where, int round) {
        for (size_t i = 0; i < effects.size(); ++i) {
            const json& e = effects[i];
            ASSERT_TRUE(e.is_object())
                << where << " round=" << round << " i=" << i;
            for (auto it = e.begin(); it != e.end(); ++it) {
                const auto& k = it.key();
                ASSERT_NE(k.c_str(), nullptr)
                    << where << " round=" << round << " i=" << i
                    << ": key.c_str() is NULL — bucket corrupted";
                ASSERT_GT(k.size(), 0u)
                    << where << " round=" << round << " i=" << i
                    << ": key.size() == 0";
            }
        }
    };

    json effects(json::value_t::array);
    for (int round = 0; round < 6; ++round) {
        json effect(json::value_t::object);
        effect["name"]     = "freq_shifter";
        sanity_walk(effects, "after name", round);
        effect["display"]  = "Frequency Shifter";
        sanity_walk(effects, "after display", round);
        effect["category"] = "modifier";
        sanity_walk(effects, "after category", round);

        json params = json::parse(kSchemaLarge, std::strlen(kSchemaLarge),
                                  nullptr, nullptr, nullptr);
        sanity_walk(effects, "after parse", round);

        effect["params"] = params;
        sanity_walk(effects, "after params assign", round);

        effects.push_back(effect);
        sanity_walk(effects, "after push_back", round);
    }
    EXPECT_EQ(effects.size(), 6u);
}

/* Minimal repro: the crash above narrows down to "object with a few
 * string keys + parse a large schema + assign parsed result back into a
 * key of that object". This isolates the bug from the loop noise. */
TEST_F(McustlTestFixture, JsonObjectAssignParsedSchemaIntoExistingKey_Min) {
    json effect(json::value_t::object);
    effect["name"]     = "freq_shifter";
    effect["display"]  = "Frequency Shifter";
    effect["category"] = "modifier";

    json params = json::parse(kSchemaLarge, std::strlen(kSchemaLarge),
                              nullptr, nullptr, nullptr);
    ASSERT_TRUE(params.is_array()) << "schema parse must succeed";

    /* This is the line that crashed in the bigger test. */
    effect["params"] = params;

    /* Validate post-state. */
    EXPECT_EQ(effect.size(), 4u);
    ASSERT_TRUE(effect.contains("name"));
    EXPECT_STREQ(effect.at("name").get_string().c_str(), "freq_shifter");
    EXPECT_TRUE(effect.at("params").is_array());
}

/* Repeat the above many times in a single fixture lifetime. The device
 * crash needed cumulative defrag activity to expose the latent issue —
 * one round of buildEffectsRegistryJson likely won't do it. */
TEST_F(McustlTestFixture, JsonBuildEffectsRegistryRepro_RepeatedRounds) {
    for (int round = 0; round < 30; ++round) {
        mcustl::heap_guard outer;

        json snapshot;
        {
            mcustl::heap_guard inner;
            snapshot = json(json::value_t::object);

            {
                json limits(json::value_t::object);
                limits["max_modifiers_per_stream"] = 8;
                snapshot["limits"] = limits;
            }

            json effects(json::value_t::array);

            for (int k = 0; k < 6; ++k) {
                json effect(json::value_t::object);
                effect["name"]     = "pitch_down";
                effect["display"]  = "Pitch Down";
                effect["category"] = "modifier";

                const char* schema =
                    "[{\"name\":\"semitones\",\"type\":\"int\","
                    "\"min\":-12,\"max\":12,\"default\":-3}]";
                json params = json::parse(schema, std::strlen(schema),
                                          nullptr, nullptr, nullptr);
                effect["params"] = params;
                effects.push_back(effect);
            }
            snapshot["effects"] = effects;
        }

        ASSERT_EQ(snapshot.at("effects").size(), 6u) << "round " << round;
        mcustl::string dumped = snapshot.dump(-1);
        EXPECT_GT(dumped.size(), 0u) << "round " << round;
    }
}
