#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(value) Require((value), __LINE__)

bool SameWide(const char16_t* left, const char16_t* right, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        if (left[i] != right[i]) return false;
    }
    return true;
}

std::size_t WideLength(const char16_t* text) {
    std::size_t count = 0;
    while (text[count] != u'\0') ++count;
    return count;
}

int Sign(int val) {
    if (val < 0) return -1;
    if (val > 0) return 1;
    return 0;
}

void TestNarrowQuery() {
    REQUIRE(_CStrxfrm_nid_postfix(nullptr, "", 0) == 0);
    REQUIRE(_CStrxfrm_nid_postfix(nullptr, "a", 0) == 1);
    REQUIRE(_CStrxfrm_nid_postfix(nullptr, "hello world", 0) == 11);
    char dummy[4] = {'x', 'x', 'x', 'x'};
    REQUIRE(_CStrxfrm_nid_postfix(dummy, "hello world", 0) == 11);
    REQUIRE(dummy[0] == 'x' && dummy[1] == 'x' && dummy[2] == 'x' && dummy[3] == 'x');
}

void TestNarrowEmpty() {
    char buffer[8];
    std::memset(buffer, 'Z', sizeof(buffer));
    REQUIRE(_CStrxfrm_nid_postfix(buffer, "", 0) == 0);
    REQUIRE(buffer[0] == 'Z');

    std::memset(buffer, 'Z', sizeof(buffer));
    REQUIRE(_CStrxfrm_nid_postfix(buffer, "", sizeof(buffer)) == 0);
    REQUIRE(buffer[0] == '\0');
    REQUIRE(buffer[1] == 'Z');
    REQUIRE(buffer[7] == 'Z');
}

void TestNarrowSized() {
    char buffer[16];
    std::memset(buffer, 'Z', sizeof(buffer));
    const char* src = "hello";
    REQUIRE(_CStrxfrm_nid_postfix(buffer, src, sizeof(buffer)) == 5);
    REQUIRE(std::strcmp(buffer, src) == 0);
    REQUIRE(buffer[5] == '\0');
    REQUIRE(buffer[6] == 'Z');
    REQUIRE(buffer[15] == 'Z');

    std::memset(buffer, 'Z', sizeof(buffer));
    const char* exactSrc = "1234567";
    REQUIRE(_CStrxfrm_nid_postfix(buffer, exactSrc, 8) == 7);
    REQUIRE(std::strcmp(buffer, exactSrc) == 0);
    REQUIRE(buffer[7] == '\0');
    REQUIRE(buffer[8] == 'Z');
}

void TestNarrowTruncation() {
    char buffer[16];
    std::memset(buffer, 'Z', sizeof(buffer));
    const char* src = "abcdefghijkl";
    REQUIRE(_CStrxfrm_nid_postfix(buffer, src, 4) == 12);
    REQUIRE(std::memcmp(buffer, "abcd", 4) == 0);
    REQUIRE(buffer[4] == 'Z');
    REQUIRE(buffer[15] == 'Z');

    std::memset(buffer, 'Z', sizeof(buffer));
    REQUIRE(_CStrxfrm_nid_postfix(buffer, src, 1) == 12);
    REQUIRE(buffer[0] == 'a');
    REQUIRE(buffer[1] == 'Z');

    std::memset(buffer, 'Z', sizeof(buffer));
    REQUIRE(_CStrxfrm_nid_postfix(buffer, src, 12) == 12);
    REQUIRE(std::memcmp(buffer, "abcdefghijkl", 12) == 0);
    REQUIRE(buffer[12] == 'Z');
}

void TestNarrowCollation() {
    const std::vector<std::pair<const char*, const char*>> pairs = {
        {"apple", "banana"},
        {"banana", "apple"},
        {"apple", "apple"},
        {"a", "aa"},
        {"aa", "a"},
        {"Hello", "hello"},
        {"test123", "test124"},
        {"", "nonempty"},
        {"nonempty", ""},
        {"", ""}
    };
    for (const auto& pair : pairs) {
        char d1[64];
        char d2[64];
        std::memset(d1, 0, sizeof(d1));
        std::memset(d2, 0, sizeof(d2));
        _CStrxfrm_nid_postfix(d1, pair.first, sizeof(d1));
        _CStrxfrm_nid_postfix(d2, pair.second, sizeof(d2));
        const int origSign = Sign(std::strcmp(pair.first, pair.second));
        const int transSign = Sign(std::strcmp(d1, d2));
        REQUIRE(origSign == transSign);
    }
}

void TestWideQuery() {
    REQUIRE(_CWcsxfrm_nid_postfix(nullptr, u"", 0) == 0);
    REQUIRE(_CWcsxfrm_nid_postfix(nullptr, u"a", 0) == 1);
    REQUIRE(_CWcsxfrm_nid_postfix(nullptr, u"wide string test", 0) == 16);
    char16_t dummy[4] = {u'x', u'x', u'x', u'x'};
    REQUIRE(_CWcsxfrm_nid_postfix(dummy, u"wide string test", 0) == 16);
    REQUIRE(dummy[0] == u'x' && dummy[1] == u'x' && dummy[2] == u'x' && dummy[3] == u'x');
}

void TestWideEmpty() {
    char16_t buffer[8];
    for (auto& ch : buffer) ch = 0xAAAA;
    REQUIRE(_CWcsxfrm_nid_postfix(buffer, u"", 0) == 0);
    REQUIRE(buffer[0] == 0xAAAA);

    for (auto& ch : buffer) ch = 0xAAAA;
    REQUIRE(_CWcsxfrm_nid_postfix(buffer, u"", 8) == 0);
    REQUIRE(buffer[0] == u'\0');
    REQUIRE(buffer[1] == 0xAAAA);
    REQUIRE(buffer[7] == 0xAAAA);
}

void TestWideSized() {
    char16_t buffer[16];
    for (auto& ch : buffer) ch = 0xAAAA;
    const char16_t* src = u"hello";
    REQUIRE(_CWcsxfrm_nid_postfix(buffer, src, 16) == 5);
    REQUIRE(SameWide(buffer, src, 5));
    REQUIRE(buffer[5] == u'\0');
    REQUIRE(buffer[6] == 0xAAAA);
    REQUIRE(buffer[15] == 0xAAAA);

    for (auto& ch : buffer) ch = 0xAAAA;
    const char16_t* exactSrc = u"1234567";
    REQUIRE(_CWcsxfrm_nid_postfix(buffer, exactSrc, 8) == 7);
    REQUIRE(SameWide(buffer, exactSrc, 7));
    REQUIRE(buffer[7] == u'\0');
    REQUIRE(buffer[8] == 0xAAAA);
}

void TestWideTruncation() {
    char16_t buffer[16];
    for (auto& ch : buffer) ch = 0xAAAA;
    const char16_t* src = u"abcdefghijkl";
    REQUIRE(_CWcsxfrm_nid_postfix(buffer, src, 4) == 12);
    REQUIRE(SameWide(buffer, u"abcd", 4));
    REQUIRE(buffer[4] == 0xAAAA);
    REQUIRE(buffer[15] == 0xAAAA);

    for (auto& ch : buffer) ch = 0xAAAA;
    REQUIRE(_CWcsxfrm_nid_postfix(buffer, src, 1) == 12);
    REQUIRE(buffer[0] == u'a');
    REQUIRE(buffer[1] == 0xAAAA);

    for (auto& ch : buffer) ch = 0xAAAA;
    REQUIRE(_CWcsxfrm_nid_postfix(buffer, src, 12) == 12);
    REQUIRE(SameWide(buffer, u"abcdefghijkl", 12));
    REQUIRE(buffer[12] == 0xAAAA);
}

void TestWideUtf16() {
    const char16_t latin[] = u"\u00e9\u00e0\u00f1";
    const char16_t cyrillic[] = u"\u041f\u0440\u0438\u0432\u0435\u0442";
    const char16_t hiragana[] = u"\u3053\u3093\u306b\u3061\u306f";
    const char16_t surrogates[] = u"\xD83D\xDE00\xD83D\xDE02";
    const char16_t mixed[] = u"Test \u00e9 \u041f \u3053 \xD83D\xDE00 end";

    const char16_t* testCases[] = {latin, cyrillic, hiragana, surrogates, mixed};
    for (const auto* src : testCases) {
        const std::size_t len = WideLength(src);
        REQUIRE(_CWcsxfrm_nid_postfix(nullptr, src, 0) == len);

        char16_t buffer[64];
        for (auto& ch : buffer) ch = 0xBEEF;
        REQUIRE(_CWcsxfrm_nid_postfix(buffer, src, 64) == len);
        REQUIRE(SameWide(buffer, src, len));
        REQUIRE(buffer[len] == u'\0');
        REQUIRE(buffer[len + 1] == 0xBEEF);

        if (len > 2) {
            for (auto& ch : buffer) ch = 0xBEEF;
            REQUIRE(_CWcsxfrm_nid_postfix(buffer, src, 2) == len);
            REQUIRE(SameWide(buffer, src, 2));
            REQUIRE(buffer[2] == 0xBEEF);
        }
    }
}

void TestWideCollation() {
    const std::vector<std::pair<const char16_t*, const char16_t*>> pairs = {
        {u"apple", u"banana"},
        {u"banana", u"apple"},
        {u"apple", u"apple"},
        {u"a", u"aa"},
        {u"aa", u"a"},
        {u"Hello", u"hello"},
        {u"\u0410", u"\u0411"},
        {u"\u0411", u"\u0410"},
        {u"\u3042", u"\u3044"},
        {u"\u3044", u"\u3042"},
        {u"", u"nonempty"},
        {u"nonempty", u""},
        {u"", u""}
    };
    for (const auto& pair : pairs) {
        char16_t d1[64];
        char16_t d2[64];
        for (auto& ch : d1) ch = 0;
        for (auto& ch : d2) ch = 0;
        _CWcsxfrm_nid_postfix(d1, pair.first, 64);
        _CWcsxfrm_nid_postfix(d2, pair.second, 64);
        const int origSign = Sign(wcscmp_nid_postfix(pair.first, pair.second));
        const int transSign = Sign(wcscmp_nid_postfix(d1, d2));
        REQUIRE(origSign == transSign);
    }
}

void RunAllTests() {
    TestNarrowQuery();
    TestNarrowEmpty();
    TestNarrowSized();
    TestNarrowTruncation();
    TestNarrowCollation();
    TestWideQuery();
    TestWideEmpty();
    TestWideSized();
    TestWideTruncation();
    TestWideUtf16();
    TestWideCollation();
}

void TestMultithreading() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(RunAllTests);
    }
    for (auto& t : threads) {
        t.join();
    }
}

}

int main() {
    RunAllTests();
    TestMultithreading();
    std::puts("guest libc internal strxfrm tests passed");
    return 0;
}
