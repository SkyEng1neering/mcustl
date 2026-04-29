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
