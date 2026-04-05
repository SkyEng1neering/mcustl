/**
 * @file test_string.cpp
 * @brief mcustl::string tests
 */

#include "mcustl_test_config.h"

// ==================== Basic Operations ====================

TEST_F(McustlTestFixture, StringDefaultConstruct) {
    mcustl::string str;
    EXPECT_EQ(str.size(), 0u);
    EXPECT_TRUE(str.empty());
}

TEST_F(McustlTestFixture, StringFromCString) {
    mcustl::string str("Hello");
    EXPECT_EQ(str.size(), 5u);
    EXPECT_STREQ(str.c_str(), "Hello");
}

TEST_F(McustlTestFixture, StringAppendCString) {
    mcustl::string str("Hello");
    EXPECT_TRUE(str.append(" World"));
    EXPECT_EQ(str.size(), 11u);
    EXPECT_STREQ(str.c_str(), "Hello World");
}

TEST_F(McustlTestFixture, StringAppendMcustlString) {
    mcustl::string a("Hello");
    mcustl::string b(" World");
    EXPECT_TRUE(a.append(b));
    EXPECT_STREQ(a.c_str(), "Hello World");
}

TEST_F(McustlTestFixture, StringPushBack) {
    mcustl::string str;
    EXPECT_TRUE(str.push_back('A'));
    EXPECT_TRUE(str.push_back('B'));
    EXPECT_TRUE(str.push_back('C'));
    EXPECT_EQ(str.size(), 3u);
    EXPECT_STREQ(str.c_str(), "ABC");
}

TEST_F(McustlTestFixture, StringPopBack) {
    mcustl::string str("ABC");
    EXPECT_TRUE(str.pop_back());
    EXPECT_STREQ(str.c_str(), "AB");
    EXPECT_TRUE(str.pop_back());
    EXPECT_STREQ(str.c_str(), "A");
    EXPECT_TRUE(str.pop_back());
    EXPECT_EQ(str.size(), 0u);
    EXPECT_FALSE(str.pop_back());
}

TEST_F(McustlTestFixture, StringClear) {
    mcustl::string str("Hello");
    str.clear();
    EXPECT_EQ(str.size(), 0u);
    EXPECT_TRUE(str.empty());
}

// ==================== Element Access ====================

TEST_F(McustlTestFixture, StringAt) {
    mcustl::string str("ABC");
    EXPECT_EQ(str.at(0), 'A');
    EXPECT_EQ(str.at(1), 'B');
    EXPECT_EQ(str.at(2), 'C');
}

TEST_F(McustlTestFixture, StringSubscript) {
    mcustl::string str("XYZ");
    EXPECT_EQ(str[0], 'X');
    EXPECT_EQ(str[1], 'Y');
    EXPECT_EQ(str[2], 'Z');
}

TEST_F(McustlTestFixture, StringFrontBack) {
    mcustl::string str("ABCDE");
    EXPECT_EQ(str.front(), 'A');
    EXPECT_EQ(str.back(), 'E');
}

// ==================== Capacity ====================

TEST_F(McustlTestFixture, StringReserve) {
    mcustl::string str;
    EXPECT_TRUE(str.reserve(100));
    EXPECT_GE(str.capacity(), 100u);
    EXPECT_EQ(str.size(), 0u);
}

TEST_F(McustlTestFixture, StringResize) {
    mcustl::string str("Hello");
    EXPECT_TRUE(str.resize(3));
    EXPECT_EQ(str.size(), 3u);
    EXPECT_STREQ(str.c_str(), "Hel");

    EXPECT_TRUE(str.resize(6, 'X'));
    EXPECT_EQ(str.size(), 6u);
    // Positions 4-5 are filled with 'X'
    EXPECT_EQ(str[4], 'X');
    EXPECT_EQ(str[5], 'X');
}

// ==================== Copy/Assignment ====================

TEST_F(McustlTestFixture, StringCopy) {
    mcustl::string original("Hello");
    mcustl::string copy = original;
    EXPECT_STREQ(copy.c_str(), "Hello");

    original[0] = 'J';
    EXPECT_EQ(copy[0], 'H');
}

TEST_F(McustlTestFixture, StringAssignment) {
    mcustl::string a("Hello");
    mcustl::string b("World");
    b = a;
    EXPECT_STREQ(b.c_str(), "Hello");
}

TEST_F(McustlTestFixture, StringAssignCString) {
    mcustl::string str;
    EXPECT_TRUE(str.assign("Test"));
    EXPECT_STREQ(str.c_str(), "Test");

    EXPECT_TRUE(str.assign("New"));
    EXPECT_STREQ(str.c_str(), "New");
}

// ==================== Operators ====================

TEST_F(McustlTestFixture, StringPlusEquals) {
    mcustl::string str("Hello");
    str += " World";
    EXPECT_STREQ(str.c_str(), "Hello World");

    str += '!';
    EXPECT_STREQ(str.c_str(), "Hello World!");
}

TEST_F(McustlTestFixture, StringConcatenation) {
    mcustl::string a("Hello");
    mcustl::string b(" World");
    mcustl::string c = a + b;
    EXPECT_STREQ(c.c_str(), "Hello World");
}

TEST_F(McustlTestFixture, StringConcatCString) {
    mcustl::string a("Hello");
    mcustl::string c = a + " World";
    EXPECT_STREQ(c.c_str(), "Hello World");
}

TEST_F(McustlTestFixture, StringEquality) {
    mcustl::string a("Hello");
    mcustl::string b("Hello");
    mcustl::string c("World");

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a != b);
}

// ==================== Edge Cases ====================

TEST_F(McustlTestFixture, StringEmpty) {
    mcustl::string str;
    EXPECT_EQ(str.size(), 0u);
    EXPECT_EQ(str.length(), 0u);
    EXPECT_TRUE(str.empty());

    EXPECT_TRUE(str.push_back('A'));
    EXPECT_FALSE(str.empty());
}

TEST_F(McustlTestFixture, StringSingleChar) {
    mcustl::string str("A");
    EXPECT_EQ(str.size(), 1u);
    EXPECT_EQ(str[0], 'A');
    EXPECT_EQ(str.front(), 'A');
    EXPECT_EQ(str.back(), 'A');
}

TEST_F(McustlTestFixture, StringSelfAppend) {
    mcustl::string str("AB");
    EXPECT_TRUE(str.append(str));
    EXPECT_STREQ(str.c_str(), "ABAB");
}

TEST_F(McustlTestFixture, StringMultipleStrings) {
    mcustl::string s1("First");
    mcustl::string s2("Second");
    mcustl::string s3("Third");

    s1.clear();
    // s2 and s3 should survive defragmentation
    EXPECT_STREQ(s2.c_str(), "Second");
    EXPECT_STREQ(s3.c_str(), "Third");
}

// ==================== Return from Function ====================

static mcustl::string make_greeting(const char* name) {
    mcustl::string s("Hello, ");
    s += name;
    s += "!";
    return s;
}

TEST_F(McustlTestFixture, StringReturnFromFunction) {
    mcustl::string str = make_greeting("World");
    EXPECT_STREQ(str.c_str(), "Hello, World!");
    EXPECT_EQ(str.size(), 13u);
}

TEST_F(McustlTestFixture, StringReturnAndModify) {
    mcustl::string str = make_greeting("Alice");
    str += " How are you?";
    EXPECT_STREQ(str.c_str(), "Hello, Alice! How are you?");
}

static mcustl::string concat_strings(const char* a, const char* b) {
    mcustl::string sa(a);
    mcustl::string sb(b);
    mcustl::string result = sa + sb;
    return result;
}

TEST_F(McustlTestFixture, StringReturnConcatenated) {
    mcustl::string str = concat_strings("Foo", "Bar");
    EXPECT_STREQ(str.c_str(), "FooBar");
}

static mcustl::string make_string_chain(int depth) {
    if (depth <= 1) {
        return make_greeting("base");
    }
    mcustl::string inner = make_string_chain(depth - 1);
    inner += "+";
    return inner;
}

TEST_F(McustlTestFixture, StringReturnNested) {
    mcustl::string str = make_string_chain(4);
    EXPECT_STREQ(str.c_str(), "Hello, base!+++");
}

TEST_F(McustlTestFixture, StringReturnAssign) {
    mcustl::string str("original");
    str = make_greeting("Bob");
    EXPECT_STREQ(str.c_str(), "Hello, Bob!");
}

TEST_F(McustlTestFixture, StringReturnMultiple) {
    mcustl::string s1 = make_greeting("A");
    mcustl::string s2 = make_greeting("B");
    mcustl::string s3 = make_greeting("C");

    EXPECT_STREQ(s1.c_str(), "Hello, A!");
    EXPECT_STREQ(s2.c_str(), "Hello, B!");
    EXPECT_STREQ(s3.c_str(), "Hello, C!");
}

// Conditional return prevents NRVO — forces move/copy
static mcustl::string make_string_conditional(bool flag) {
    mcustl::string a("alpha");
    mcustl::string b("beta-gamma");
    if (flag) return a;
    return b;
}

TEST_F(McustlTestFixture, StringReturnConditionalTrue) {
    mcustl::string str = make_string_conditional(true);
    EXPECT_STREQ(str.c_str(), "alpha");
    str += "-suffix";
    EXPECT_STREQ(str.c_str(), "alpha-suffix");
}

TEST_F(McustlTestFixture, StringReturnConditionalFalse) {
    mcustl::string str = make_string_conditional(false);
    EXPECT_STREQ(str.c_str(), "beta-gamma");
    str += "!";
    EXPECT_STREQ(str.c_str(), "beta-gamma!");
}
