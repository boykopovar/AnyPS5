#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <climits>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("Guest string check failed\n", stderr);
        std::abort();
    }
}

static bool CheckMemcpyOverlap() {
    bool correct = true;
    for (const auto offset : {0, 1, -1, 3, -3}) {
        unsigned char bytes[16];
        std::memset(bytes, 0x5a, sizeof(bytes));
        auto* destination = bytes + 4;
        const auto* source = destination + offset;
        const int error = memcpy_s_nid_postfix(destination, 8, source, 4);
        bool cleared = true;
        for (unsigned i = 4; i < 12; ++i) cleared &= bytes[i] == 0;
        const bool matches = error == 22 && cleared && bytes[3] == 0x5a && bytes[12] == 0x5a;
        if (!matches) std::fprintf(stderr, "memcpy_s overlap %+d: expected EINVAL 22 and eight zero bytes, received %d\n", offset, error);
        correct &= matches;
    }
    unsigned char adjacent[] = {1, 2, 3, 4, 5, 6, 7, 8};
    Require(memcpy_s_nid_postfix(adjacent, 4, adjacent + 4, 4) == 0);
    Require(std::memcmp(adjacent, adjacent + 4, 4) == 0);
    const unsigned char original[] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::memcpy(adjacent, original, sizeof(adjacent));
    Require(memcpy_s_nid_postfix(adjacent + 4, 4, adjacent, 4) == 0);
    const unsigned char unchanged[] = {1, 2, 3, 4, 1, 2, 3, 4};
    Require(std::memcmp(adjacent, unchanged, sizeof(adjacent)) == 0);
    Require(memcpy_s_nid_postfix(adjacent, sizeof(adjacent), adjacent, 0) == 0);
    Require(std::memcmp(adjacent, unchanged, sizeof(adjacent)) == 0);
    return correct;
}

static int Sign(int value) {
    return value < 0 ? -1 : value > 0 ? 1 : 0;
}

static void CheckStandardStrings() {
    Require(strcmp_nid_postfix("abc", "abc") == 0 && Sign(strcmp_nid_postfix("abc", "abd")) == -1 && Sign(strcmp_nid_postfix("abc", "ab")) == 1);
    Require(Sign(strcmp_nid_postfix("\x80", "\x7f")) == 1);
    Require(strncmp_nid_postfix("abcX", "abcY", 3) == 0 && Sign(strncmp_nid_postfix("abcX", "abcY", 4)) == -1 && strncmp_nid_postfix("a", "b", 0) == 0);
    Require(strcoll_nid_postfix("same", "same") == 0 && Sign(strcoll_nid_postfix("B", "a")) == -1);
    Require(strlen_nid_postfix("") == 0 && strlen_nid_postfix("seven 7") == 7);

    char buffer[16];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(strcpy_nid_postfix(buffer, "ab") == buffer && std::memcmp(buffer, "ab\0x", 4) == 0);
    Require(strcat_nid_postfix(buffer, "cd") == buffer && std::strcmp(buffer, "abcd") == 0);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(strncpy_nid_postfix(buffer, "ab", 5) == buffer && std::memcmp(buffer, "ab\0\0\0x", 6) == 0);
    std::memset(buffer, 'x', sizeof(buffer));
    Require(strncpy_nid_postfix(buffer, "abcdef", 3) == buffer && std::memcmp(buffer, "abcx", 4) == 0);

    const char* text = "needle in a haystack";
    Require(strstr_nid_postfix(text, "hay") == text + 12 && strstr_nid_postfix(text, "") == text && strstr_nid_postfix(text, "hey") == nullptr);
    Require(strspn_nid_postfix("aabbc", "ab") == 4 && strspn_nid_postfix("xyz", "ab") == 0 && strspn_nid_postfix("ab", "") == 0);

    unsigned char bytes[6] = {1, 2, 3, 4, 5, 6};
    const unsigned char same[6] = {1, 2, 3, 4, 5, 6};
    Require(bcmp_nid_postfix(bytes, same, 6) == 0 && bcmp_nid_postfix(bytes, "zz", 0) == 0);
    bytes[5] = 9;
    Require(bcmp_nid_postfix(bytes, same, 6) != 0 && bcmp_nid_postfix(bytes, same, 5) == 0);
    bzero_nid_postfix(bytes + 1, 4);
    Require(bytes[0] == 1 && bytes[1] == 0 && bytes[4] == 0 && bytes[5] == 9);
}

static void CheckSecureCopySizeLimits() {
    using Copy = int (APS5_VABI*)(void*, std::size_t, const void*, std::size_t);
    constexpr std::size_t RsizeMax = SIZE_MAX >> 1;
    const std::array<unsigned char, 8> original = {1, 2, 3, 4, 5, 6, 7, 8};
    const unsigned char source[] = {9, 10, 11, 12};
    for (Copy copy : {memcpy_s_nid_postfix, memmove_s_nid_postfix}) {
        for (const auto size : {RsizeMax + 1, SIZE_MAX}) {
            for (const auto count : {std::size_t{1}, std::size_t{0}, RsizeMax + 1, SIZE_MAX}) {
                for (const void* input : {static_cast<const void*>(source), static_cast<const void*>(nullptr)}) {
                    auto bytes = original;
                    Require(copy(bytes.data() + 2, size, input, count) != 0);
                    Require(bytes == original);
                }
            }
        }
        for (const auto count : {RsizeMax + 1, SIZE_MAX}) {
            auto bytes = original;
            Require(copy(bytes.data() + 2, 4, source, count) != 0);
            const std::array<unsigned char, 8> cleared = {1, 2, 0, 0, 0, 0, 7, 8};
            Require(bytes == cleared);
        }
        auto bytes = original;
        Require(copy(bytes.data() + 2, RsizeMax, source, 1) == 0);
        const std::array<unsigned char, 8> copied = {1, 2, 9, 4, 5, 6, 7, 8};
        Require(bytes == copied);
        Require(copy(bytes.data() + 2, 4, source, 0) == 0 && bytes == copied);
        Require(copy(nullptr, SIZE_MAX, source, 1) == 22);
        bytes = original;
        Require(copy(bytes.data() + 2, 4, nullptr, 1) == 22);
        const std::array<unsigned char, 8> cleared = {1, 2, 0, 0, 0, 0, 7, 8};
        Require(bytes == cleared);
    }
}

static void CheckBoundsCheckedFunctions() {
    char small[4] = "zz";
    Require(strcpy_s_nid_postfix(small, sizeof(small), "abc") == 0 && std::strcmp(small, "abc") == 0);
    Require(strcpy_s_nid_postfix(small, sizeof(small), "abcd") == 34 && small[0] == '\0');
    Require(strcpy_s_nid_postfix(nullptr, 4, "a") == 22);
    char joined[8] = "ab";
    Require(strcat_s_nid_postfix(joined, sizeof(joined), "cd") == 0 && std::strcmp(joined, "abcd") == 0);
    Require(strncat_s_nid_postfix(joined, sizeof(joined), "efgh", 2) == 0 && std::strcmp(joined, "abcdef") == 0);
    Require(strcat_s_nid_postfix(joined, sizeof(joined), "gh") == 34 && joined[0] == '\0');
    char bytes[4] = {1, 2, 3, 4};
    const char source[4] = {5, 6, 7, 8};
    Require(memcpy_s_nid_postfix(bytes, sizeof(bytes), source, 2) == 0 && bytes[0] == 5 && bytes[2] == 3);
    Require(memcpy_s_nid_postfix(bytes, 2, source, 4) == 34 && bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 3);
    char overlap[6] = "abcde";
    Require(memmove_s_nid_postfix(overlap + 1, 5, overlap, 3) == 0 && std::strcmp(overlap, "aabce") == 0);
    char strOverlap[16] = "abcdef";
    Require(strncpy_s_nid_postfix(strOverlap + 1, 10, strOverlap, 3) == 22 && strOverlap[1] == '\0');
    char catOverlap[16] = "abcdef";
    Require(strncat_s_nid_postfix(catOverlap + 3, 10, catOverlap, 3) == 22 && catOverlap[3] == '\0');
    char nonOverlap[16] = {};
    std::strcpy(nonOverlap + 8, "cd");
    Require(strcpy_s_nid_postfix(nonOverlap, SIZE_MAX >> 1, nonOverlap + 8) == 0 && std::strcmp(nonOverlap, "cd") == 0);
    Require(memset_s_nid_postfix(bytes, sizeof(bytes), 9, 8) == 22 && bytes[0] == 9 && bytes[3] == 9);
    char sentinel[4] = {1, 2, 3, 4};
    Require(memset_s_nid_postfix(sentinel, sizeof(sentinel), 7, 2) == 0 && sentinel[1] == 7 && sentinel[2] == 3);
    Require(memset_s_nid_postfix(sentinel, (SIZE_MAX >> 1) + 1, 0, 1) == 22 && sentinel[0] == 7);
    Require(memset_s_nid_postfix(sentinel, sizeof(sentinel), 0, (SIZE_MAX >> 1) + 1) == 22 && sentinel[0] == 7 && sentinel[3] == 4);
    Require(memset_s_nid_postfix(sentinel, SIZE_MAX >> 1, 5, 1) == 0 && sentinel[0] == 5 && sentinel[1] == 7);
    Require(memset_s_nid_postfix(nullptr, sizeof(sentinel), 0, 1) == 22);
    Require(strnstr_nid_postfix("haystack", "st", 4) == nullptr);
    const char haystack[] = "haystack";
    Require(strnstr_nid_postfix(haystack, "st", 6) == haystack + 3);
    char formatted[8];
    Require(snprintf_s_nid_postfix(formatted, sizeof(formatted), "%d-%s", 42, "x") == 4 && std::strcmp(formatted, "42-x") == 0);
}

static void CheckSecureStringSizeLimits() {
    constexpr std::size_t RsizeMax = SIZE_MAX >> 1;
    const std::array<char, 8> original = {'!', 'a', 'b', '\0', 'x', 'y', 'z', '!'};
    for (const auto size : {RsizeMax + 1, SIZE_MAX}) {
        for (const char* source : {"cd", "", static_cast<const char*>(nullptr)}) {
            for (const auto copy : {strcpy_s_nid_postfix, strcat_s_nid_postfix}) {
                auto bytes = original;
                Require(copy(bytes.data() + 1, size, source) != 0);
                Require(bytes == original);
            }
            for (const auto copy : {strncpy_s_nid_postfix, strncat_s_nid_postfix}) {
                for (const auto count : {std::size_t{0}, std::size_t{1}, std::size_t{3}}) {
                    auto bytes = original;
                    Require(copy(bytes.data() + 1, size, source, count) != 0);
                    Require(bytes == original);
                }
            }
        }
    }
    auto bytes = original;
    Require(strcpy_s_nid_postfix(bytes.data() + 1, RsizeMax, "cd") == 0);
    Require(bytes[0] == '!' && std::strcmp(bytes.data() + 1, "cd") == 0 && bytes[4] == 'x');
    bytes = original;
    Require(strcat_s_nid_postfix(bytes.data() + 1, RsizeMax, "cd") == 0);
    Require(bytes[0] == '!' && std::strcmp(bytes.data() + 1, "abcd") == 0 && bytes[6] == 'z');
}

static void CheckCaseInsensitiveComparison() {
    Require(strcoll_nid_postfix("a", "z") == -25 && strcoll_nid_postfix("z", "a") == 25 && strcoll_nid_postfix("ab", "abc") == -99);
    Require(strcoll_nid_postfix("A", "a") == -32 && strcoll_nid_postfix("same", "same") == 0 && strcoll_nid_postfix("\xff", "\x01") == 254);
    Require(strcasecmp_nid_postfix("", "") == 0 && strcasecmp_nid_postfix("AbC", "aBc") == 0);
    Require(strcasecmp_nid_postfix("A", "b") == -1 && strcasecmp_nid_postfix("b", "A") == 1);
    Require(strcasecmp_nid_postfix("ab", "A") == 1 && strcasecmp_nid_postfix("AB", "a") == 1);
    Require(strcasecmp_nid_postfix("A", "aB") == -1 && strcasecmp_nid_postfix("a", "AB") == -1);
    Require(strcasecmp_nid_postfix("", "Z") == -1 && strcasecmp_nid_postfix("Z", "") == 1);
    Require(strncasecmp_nid_postfix("abc", "ABD", 2) == 0 && strncasecmp_nid_postfix("abc", "ABD", 3) == -1);
    Require(strncasecmp_nid_postfix("Ab", "a", 2) == 1 && strncasecmp_nid_postfix("a", "AB", 5) == -1);
    Require(strncasecmp_nid_postfix("Q", "", 1) == 1 && strncasecmp_nid_postfix("", "Q", 1) == -1);
    Require(strncasecmp_nid_postfix("x", "y", 0) == 0 && strncasecmp_nid_postfix("same", "SAME", 99) == 0);
}

static void CheckSscanfS() {
    int number = 0;
    char word[4] = "zz";
    char letter = 0;
    char value[8] = {};
    Require(sscanf_s_nid_postfix(" 12 abc x", "%d %s %c", &number, word, 4u, &letter, 1u) == 3 && number == 12 && std::strcmp(word, "abc") == 0 && letter == 'x');
    Require(sscanf_s_nid_postfix("12 abcd", "%d %s", &number, word, 4u) == 1 && word[0] == '\0');
    Require(sscanf_s_nid_postfix("key=val", "%3[a-z]=%3s", word, 4u, value, 8u) == 2 && std::strcmp(word, "key") == 0 && std::strcmp(value, "val") == 0);
    Require(sscanf_s_nid_postfix("abcdef", "%3s", word, 4u) == 1 && std::strcmp(word, "abc") == 0);
    Require(sscanf_s_nid_postfix("abcdef", "%3s", value, 8u) == 1 && std::strcmp(value, "abc") == 0);
    Require(sscanf_s_nid_postfix("abcdef", "%3[a-z]", value, 8u) == 1 && std::strcmp(value, "abc") == 0);
    Require(sscanf_s_nid_postfix("2024ABCD 7", "%4s%s", word, 4u, value, 8u) == 0 && word[0] == '\0');
    Require(sscanf_s_nid_postfix("2024ABCD", "%3s%4s", word, 4u, value, 8u) == 2 && std::strcmp(word, "202") == 0 && std::strcmp(value, "4ABC") == 0);
    int position = 0;
    Require(sscanf_s_nid_postfix("7 %", "%d %%%n", &number, &position) == 1 && position == 3);
    Require(sscanf_s_nid_postfix("   ", "%d", &number) == EOF);
    Require(sscanf_s_nid_postfix("x", "%d", &number) == 0);
}

static bool CheckDirnameLengths() {
    const std::string longLeaf(4096, 'x');
    const std::string longestParent(1023, 'p');
    const std::string tooLongParent(1024, 'q');
    struct Case {
        std::string path;
        const char* expected;
    };
    const Case cases[] = {
        {std::string(1023, 'x'), "."},
        {std::string(1024, 'x'), "."},
        {longLeaf, "."},
        {"/" + longLeaf, "/"},
        {"one/" + longLeaf, "one"},
        {"/one/" + longLeaf, "/one"},
        {"one//" + longLeaf + "///", "one"},
        {"/one//" + longLeaf + "///", "/one"},
        {longLeaf + "///", "."},
        {std::string(4096, '/'), "/"},
        {longestParent + "/leaf", longestParent.c_str()},
        {longestParent + "//leaf///", longestParent.c_str()},
        {longestParent + "/", "."},
        {tooLongParent + "/leaf", nullptr},
        {tooLongParent + "//leaf///", nullptr},
        {longLeaf + "/leaf", nullptr},
    };
    bool passed = true;
    for (const auto& test : cases) {
        const std::string original = test.path;
        *__error_nid_postfix() = 77;
        const char* result = dirname_nid_postfix(test.path.c_str());
        const bool matches = test.expected
            ? result != nullptr && std::strcmp(result, test.expected) == 0 && *__error_nid_postfix() == 77
            : result == nullptr && *__error_nid_postfix() == 63;
        if (!matches || test.path != original) {
            std::fprintf(stderr, "dirname input length %zu: expected output length %zu, received %s, errno %d\n",
                test.path.size(), test.expected ? std::strlen(test.expected) : 0,
                result ? "a directory" : "null", *__error_nid_postfix());
            passed = false;
        }
    }
    return passed;
}

static void CheckBcopy() {
    constexpr std::array<unsigned char, 12> original = {0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187};
    for (std::size_t source = 0; source < original.size(); ++source) {
        for (std::size_t destination = 0; destination < original.size(); ++destination) {
            for (std::size_t count = 0; count <= original.size() - std::max(source, destination); ++count) {
                auto actual = original;
                auto expected = original;
                for (std::size_t i = 0; i < count; ++i) expected[destination + i] = original[source + i];
                bcopy_nid_postfix(actual.data() + source, actual.data() + destination, count);

                Require(actual == expected);
            }
        }
    }
    auto destination = original;
    destination.fill(0xff);
    bcopy_nid_postfix(original.data(), destination.data(), original.size());

    Require(destination == original);
    bcopy_nid_postfix(nullptr, nullptr, 0);

}

static void CheckStrtonum() {
    struct Valid {
        const char* text;
        long long value;
    };
    for (const auto& test : {Valid{"0", 0}, Valid{"-0", 0}, Valid{"  +42", 42}, Valid{"010", 10},
             Valid{"9223372036854775807", LLONG_MAX}, Valid{"-9223372036854775808", LLONG_MIN}}) {
        const char* error = "unchanged";
        *__error_nid_postfix() = 13;
        Require(strtonum_nid_postfix(test.text, LLONG_MIN, LLONG_MAX, &error) == test.value);
        Require(error == nullptr && *__error_nid_postfix() == 13);
    }
    for (const auto& test : {Valid{"-5", -5}, Valid{"5", 5}, Valid{"0", 0}}) {
        const char* error = "unchanged";
        Require(strtonum_nid_postfix(test.text, -5, 5, &error) == test.value && error == nullptr);
        Require(strtonum_nid_postfix(test.text, test.value, test.value, &error) == test.value && error == nullptr);
    }
    const auto fails = [](const char* text, long long minimum, long long maximum, const char* expected, int number) {
        const char* error = nullptr;
        *__error_nid_postfix() = 13;
        Require(strtonum_nid_postfix(text, minimum, maximum, &error) == 0);
        Require(error != nullptr && std::strcmp(error, expected) == 0 && *__error_nid_postfix() == number);
        *__error_nid_postfix() = 13;
        Require(strtonum_nid_postfix(text, minimum, maximum, nullptr) == 0 && *__error_nid_postfix() == number);
    };
    for (const char* text : {"", " ", "+", "-", "--1", "0x10", "0b10", "1x", "42 ", "1\n",
             "9223372036854775808x", "-9223372036854775809x"})
        fails(text, LLONG_MIN, LLONG_MAX, "invalid", 22);
    fails("0", 1, -1, "invalid", 22);
    fails(nullptr, 1, -1, "invalid", 22);
    fails("9223372036854775808", 1, -1, "invalid", 22);
    fails("-6", -5, 5, "too small", 34);
    fails("6", -5, 5, "too large", 34);
    fails("0", 1, 5, "too small", 34);
    fails("0", -5, -1, "too large", 34);
    fails("-9223372036854775809", LLONG_MIN, LLONG_MAX, "too small", 34);
    fails("9223372036854775808", LLONG_MIN, LLONG_MAX, "too large", 34);
    fails("-9999999999999999999999999999999999", LLONG_MIN, LLONG_MAX, "too small", 34);
    fails("9999999999999999999999999999999999", LLONG_MIN, LLONG_MAX, "too large", 34);
    *__error_nid_postfix() = 34;
    Require(strtonum_nid_postfix("42", 0, 100, nullptr) == 42 && *__error_nid_postfix() == 34);
}

static void CheckCharacterConversionInSearches() {
    const char bytes[] = "a\xe9" "b\xe9" "c";
    Require(strchr_nid_postfix(bytes, 0xe9) == bytes + 1);
    Require(strchr_nid_postfix(bytes, 0x1e9) == bytes + 1);
    Require(strchr_nid_postfix(bytes, -23) == bytes + 1);
    Require(strrchr_nid_postfix(bytes, 0xe9) == bytes + 3);
    Require(strrchr_nid_postfix(bytes, 0x1e9) == bytes + 3);
    Require(strrchr_nid_postfix(bytes, -23) == bytes + 3);
    const char path[] = "a/b/c";
    Require(strrchr_nid_postfix(path, 0x12f) == path + 3);
    Require(strrchr_nid_postfix(path, 0) == path + 5);
    Require(strrchr_nid_postfix(path, 'z') == nullptr);
}

int main() {
    CheckStandardStrings();
    CheckStrtonum();
    CheckBcopy();
    if (!CheckDirnameLengths()) return 1;
    CheckCharacterConversionInSearches();
    CheckBoundsCheckedFunctions();
    CheckCaseInsensitiveComparison();
    CheckSscanfS();
    Require(std::strcmp(dirname_nid_postfix(nullptr), ".") == 0);
    char emptyPath[] = "";
    Require(std::strcmp(dirname_nid_postfix(emptyPath), ".") == 0);
    Require(std::strcmp(dirname_nid_postfix("file"), ".") == 0);
    const char nested[] = "one/two///";
    Require(std::strcmp(dirname_nid_postfix(nested), "one") == 0 && std::strcmp(nested, "one/two///") == 0);
    char repeatedSeparators[] = "one//two";
    Require(std::strcmp(dirname_nid_postfix(repeatedSeparators), "one") == 0);
    char absoluteRepeatedSeparators[] = "/one//two";
    Require(std::strcmp(dirname_nid_postfix(absoluteRepeatedSeparators), "/one") == 0);
    char rootPath[] = "/";
    Require(std::strcmp(dirname_nid_postfix(rootPath), "/") == 0);
    char trailingSlash[] = "foo/";
    Require(std::strcmp(dirname_nid_postfix(trailingSlash), ".") == 0);
    char absolute[] = "/one/two";
    Require(std::strcmp(dirname_nid_postfix(absolute), "/one") == 0);
    char rootChild[] = "/file";
    Require(std::strcmp(dirname_nid_postfix(rootChild), "/") == 0);
    char onlySlashes[] = "////";
    Require(std::strcmp(dirname_nid_postfix(onlySlashes), "/") == 0);
    Require(std::strcmp(basename_nid_postfix(nullptr), ".") == 0);
    Require(std::strcmp(basename_nid_postfix(""), ".") == 0);
    Require(std::strcmp(basename_nid_postfix("////"), "/") == 0);
    const char path[] = "/one/two///";
    Require(std::strcmp(basename_nid_postfix(path), "two") == 0);
    Require(std::strcmp(path, "/one/two///") == 0);
    Require(std::strcmp(basename_nid_postfix("one\\two"), "one\\two") == 0);
    const std::string longName(1024, 'x');
    Require(basename_nid_postfix(longName.c_str()) == nullptr && *__error_nid_postfix() == 63);
    char baseBuffer[1024];
    Require(basename_r_nid_postfix(path, baseBuffer) == baseBuffer && std::strcmp(baseBuffer, "two") == 0);
    Require(basename_r_nid_postfix(nullptr, baseBuffer) == baseBuffer && std::strcmp(baseBuffer, ".") == 0);
    Require(basename_r_nid_postfix("///", baseBuffer) == baseBuffer && std::strcmp(baseBuffer, "/") == 0);
    Require(basename_r_nid_postfix("plain", baseBuffer) == baseBuffer && std::strcmp(baseBuffer, "plain") == 0);
    const std::string longest(1023, 'y');
    Require(basename_r_nid_postfix(("/a/" + longest).c_str(), baseBuffer) == baseBuffer && std::strlen(baseBuffer) == 1023);
    *__error_nid_postfix() = 0;
    Require(basename_r_nid_postfix(longName.c_str(), baseBuffer) == nullptr && *__error_nid_postfix() == 63);
    const unsigned char loopbackBytes[4] = {127, 0, 0, 1};
    unsigned int loopback;
    std::memcpy(&loopback, loopbackBytes, sizeof(loopback));
    char* const dotted = __inet_ntoa_nid_postfix(loopback);
    Require(std::strcmp(dotted, "127.0.0.1") == 0);
    Require(__inet_ntoa_nid_postfix(0xffffffffu) == dotted && std::strcmp(dotted, "255.255.255.255") == 0);
    Require(std::strcmp(__inet_ntoa_nid_postfix(0), "0.0.0.0") == 0);
    const unsigned char mixedBytes[4] = {10, 200, 3, 45};
    unsigned int mixed;
    std::memcpy(&mixed, mixedBytes, sizeof(mixed));
    Require(std::strcmp(__inet_ntoa_nid_postfix(mixed), "10.200.3.45") == 0);
    const unsigned char paddingBytes[4] = {100, 9, 99, 0};
    unsigned int padding;
    std::memcpy(&padding, paddingBytes, sizeof(padding));
    Require(std::strcmp(__inet_ntoa_nid_postfix(padding), "100.9.99.0") == 0);
    const char bounded[] = {'a', 'b', 'c'};
    Require(strnlen_nid_postfix(bounded, 0) == 0);
    Require(strnlen_nid_postfix(bounded, sizeof(bounded)) == 3);
    Require(strnlen_nid_postfix("a", 8) == 1);
    Require(strnlen_s_nid_postfix(nullptr, 8) == 0);
    Require(strnlen_s_nid_postfix("abc", 8) == 3 && strnlen_s_nid_postfix("abcdef", 4) == 4);
    char truncated[] = "abXX";
    Require(strlcat_nid_postfix(truncated, "cd", 2) == 4);
    Require(std::strcmp(truncated, "abXX") == 0);
    char buffer[8] = "ab";
    Require(strlcat_nid_postfix(buffer, "cdefgh", sizeof(buffer)) == 8);
    Require(std::strcmp(buffer, "abcdefg") == 0);
    Require(strlcat_nid_postfix(buffer, "xyz", 0) == 3);
    buffer[0] = '\0';
    Require(strlcat_nid_postfix(buffer, "x", 1) == 1 && buffer[0] == '\0');
    char chained[8] = "zzzzzzz";
    char* end = stpcpy_nid_postfix(chained, "ab");
    Require(end == chained + 2 && *end == '\0' && chained[3] == 'z');
    end = stpcpy_nid_postfix(end, "cd");
    Require(end == chained + 4 && std::strcmp(chained, "abcd") == 0);
    Require(stpcpy_nid_postfix(end, "") == end && chained[5] == 'z');
    Require(strncat_nid_postfix(buffer, "xyz", 2) == buffer);
    Require(std::strcmp(buffer, "xy") == 0);
    char transformed[8] = "zzzzzzz";
    Require(strxfrm_nid_postfix(transformed, "abc", sizeof(transformed)) == 3);
    Require(std::strcmp(transformed, "abc") == 0 && transformed[4] == 'z');
    std::memset(transformed, 'z', sizeof(transformed));
    Require(strxfrm_nid_postfix(transformed, "abcdefgh", 4) == 8);
    Require(std::memcmp(transformed, "abcd", 4) == 0 && transformed[4] == 'z');
    std::memset(transformed, 'z', sizeof(transformed));
    Require(strxfrm_nid_postfix(transformed, "abcdefg", sizeof(transformed)) == 7 && std::strcmp(transformed, "abcdefg") == 0);
    std::memset(transformed, 'z', sizeof(transformed));
    Require(strxfrm_nid_postfix(transformed, "abcdefgh", sizeof(transformed)) == 8 && std::memcmp(transformed, "abcdefgh", 8) == 0);
    std::memset(transformed, 'z', sizeof(transformed));
    Require(strxfrm_nid_postfix(transformed, "", sizeof(transformed)) == 0 && transformed[0] == '\0' && transformed[1] == 'z');
    Require(strxfrm_nid_postfix(transformed, "abc", 1) == 3 && transformed[0] == 'a' && transformed[1] == 'z');
    Require(strxfrm_nid_postfix(nullptr, "abc", 0) == 3);
    Require(strpbrk_nid_postfix(buffer, "ay") == buffer + 1);
    Require(strpbrk_nid_postfix(buffer, "") == nullptr);
    Require(strcspn_nid_postfix(buffer, "y") == 1);
    const auto inetAton = [](const char* text, const char* expected) {
        unsigned char address[4] = {0xA5, 0xA5, 0xA5, 0xA5};
        if (__inet_aton_nid_postfix(text, address) != 1) return false;
        char formatted[16];
        std::snprintf(formatted, sizeof(formatted), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
        return std::strcmp(formatted, expected) == 0;
    };
    Require(inetAton("192.0.2.42", "192.0.2.42"));
    Require(inetAton("10.1.2", "10.1.0.2"));
    Require(inetAton("127.1", "127.0.0.1"));
    Require(inetAton("3232235777", "192.168.1.1"));
    Require(inetAton("0x7f.0.0.0x1", "127.0.0.1"));
    Require(inetAton("0377.0.0.010", "255.0.0.8"));
    Require(inetAton("1.2.3.4 trailing", "1.2.3.4"));
    Require(inetAton("1.2.3.4\n", "1.2.3.4"));
    Require(__inet_aton_nid_postfix("1.2.3.4", nullptr) == 1);
    for (const char* invalid : {"", " 1.2.3.4", "1.2.3.4.5", "256.1.1.1", "1.2.3.256", "1.2.65536", "08", "1..2", "a.b.c.d",
             "1.2.3.4x", "0x", "1.2.3.", "-1"}) {
        unsigned char address[4] = {0xA5, 0xA5, 0xA5, 0xA5};
        Require(__inet_aton_nid_postfix(invalid, address) == 0);
        Require(address[0] == 0xA5 && address[3] == 0xA5);
        Require(__inet_addr_nid_postfix(invalid) == 0xffffffff);
    }
    const auto inetAddr = [](const char* text, std::array<unsigned char, 4> expected) {
        const std::uint32_t value = __inet_addr_nid_postfix(text);
        std::array<unsigned char, 4> bytes{};
        std::memcpy(bytes.data(), &value, sizeof(value));
        return bytes == expected;
    };
    Require(inetAddr("192.0.2.42", {192, 0, 2, 42}));
    Require(inetAddr("127.1", {127, 0, 0, 1}));
    Require(inetAddr("0x7f.0.0.0x1", {127, 0, 0, 1}));
    Require(inetAddr("0.0.0.0", {0, 0, 0, 0}));
    Require(inetAddr("255.255.255.255", {255, 255, 255, 255}));
    Require(inetAddr("4294967296", {0, 0, 0, 0}));
    Require(inetAddr("18446744073709551617", {0, 0, 0, 1}));
    char first[] = ",a,,b,";
    char second[] = "x:y";
    char* firstState = nullptr;
    char* secondState = nullptr;
    Require(std::strcmp(strtok_r_nid_postfix(first, ",", &firstState), "a") == 0);
    Require(firstState == first + 3);
    Require(std::strcmp(strtok_r_nid_postfix(second, ":", &secondState), "x") == 0);
    Require(secondState == second + 2);
    Require(std::strcmp(strtok_r_nid_postfix(nullptr, ",", &firstState), "b") == 0);
    Require(firstState == first + 6);
    Require(strtok_r_nid_postfix(nullptr, ",", &firstState) == nullptr);
    if (firstState != nullptr) std::fputs("strtok_r retained the state after exhausting delimiters\n", stderr);
    Require(firstState == nullptr);
    Require(strtok_r_nid_postfix(nullptr, ",", &firstState) == nullptr);
    Require(std::strcmp(strtok_r_nid_postfix(nullptr, "", &secondState), "y") == 0);
    Require(secondState == nullptr);
    char tokenEnd[8] = "last";
    char* finishedState = first;
    *__error_nid_postfix() = 77;
    Require(strtok_r_nid_postfix(tokenEnd, ",", &finishedState) == tokenEnd && finishedState == nullptr);
    tokenEnd[4] = 'x';
    tokenEnd[5] = '\0';
    Require(strtok_r_nid_postfix(nullptr, ",", &finishedState) == nullptr && finishedState == nullptr);
    for (const char* input : {"", ",", ",,,"}) {
        char noTokens[8];
        std::strcpy(noTokens, input);
        finishedState = first;
        Require(strtok_r_nid_postfix(noTokens, ",", &finishedState) == nullptr && finishedState == nullptr);
        Require(strtok_r_nid_postfix(nullptr, ",", &finishedState) == nullptr);
    }
    Require(*__error_nid_postfix() == 77);
    char hostTokens[] = "host:next";
    Require(std::strcmp(std::strtok(hostTokens, ":"), "host") == 0);
    char guestTokens[] = ",one,,two:three";
    Require(std::strcmp(strtok_nid_postfix(guestTokens, ","), "one") == 0);
    Require(std::strcmp(strtok_nid_postfix(nullptr, ":,"), "two") == 0);
    Require(std::strcmp(strtok_nid_postfix(nullptr, ""), "three") == 0);
    Require(strtok_nid_postfix(nullptr, ",") == nullptr);
    Require(strtok_nid_postfix(nullptr, ",") == nullptr);
    Require(std::strcmp(std::strtok(nullptr, ":"), "next") == 0);
    char fields[] = ",a,,b;c";
    char* cursor = fields;
    Require(strsep_nid_postfix(&cursor, ",;") == fields && fields[0] == '\0' && cursor == fields + 1);
    Require(std::strcmp(strsep_nid_postfix(&cursor, ",;"), "a") == 0);
    Require(std::strcmp(strsep_nid_postfix(&cursor, ",;"), "") == 0);
    Require(std::strcmp(strsep_nid_postfix(&cursor, ",;"), "b") == 0);
    char* last = cursor;
    Require(strsep_nid_postfix(&cursor, ",;") == last && std::strcmp(last, "c") == 0 && cursor == nullptr);
    Require(strsep_nid_postfix(&cursor, ",;") == nullptr && cursor == nullptr);
    char whole[] = "a,b";
    cursor = whole;
    Require(strsep_nid_postfix(&cursor, "") == whole && std::strcmp(whole, "a,b") == 0 && cursor == nullptr);
    char empty[] = "";
    cursor = empty;
    Require(strsep_nid_postfix(&cursor, ",") == empty && cursor == nullptr);
    const char text[] = "aABAbC";
    Require(strcasestr_nid_postfix(text, "ababc") == text + 1);
    Require(strcasestr_nid_postfix(text, "") == text);
    Require(strcasestr_nid_postfix(text, "abcdef") == nullptr);
    Require(strcasestr_nid_postfix("", "a") == nullptr);
    const char highBytes[] = {static_cast<char>(0xff), 'A', 0};
    Require(strcasestr_nid_postfix(highBytes, "a") == highBytes + 1);
    const char hay[] = "hello world";
    Require(memmem_nid_postfix(hay, 11, "world", 5) == hay + 6);
    Require(memmem_nid_postfix(hay, 11, "world", 0) == hay);
    Require(memmem_nid_postfix(hay, 5, "world", 5) == nullptr);
    Require(memmem_nid_postfix(hay, 11, "xyz", 3) == nullptr);
    char dest[16] = {};
    Require(memccpy_nid_postfix(dest, "abc:def", ':', 8) == dest + 4);
    Require(std::memcmp(dest, "abc:", 4) == 0);
    Require(memccpy_nid_postfix(dest, "abcdef", ':', 6) == nullptr);
    Require(memccpy_nid_postfix(dest, "abcdef", ':', 0) == nullptr);
    const char safeA[] = "abcdef";
    const char safeB[] = "abcdeg";
    const char safeC[] = "abcdef";
    Require(timingsafe_memcmp_nid_postfix(safeA, safeC, 6) == 0);
    Require(timingsafe_memcmp_nid_postfix(safeA, safeB, 6) < 0);
    Require(timingsafe_memcmp_nid_postfix(safeB, safeA, 6) > 0);
    Require(timingsafe_memcmp_nid_postfix(safeA, safeB, 5) == 0);
    Require(timingsafe_memcmp_nid_postfix(safeA, safeB, 0) == 0);
    const unsigned char highA[] = {0x00, 0x80};
    const unsigned char highB[] = {0x00, 0x7f};
    Require(timingsafe_memcmp_nid_postfix(highA, highB, 2) > 0);
    Require(timingsafe_memcmp_nid_postfix(highB, highA, 2) < 0);
    CheckSecureCopySizeLimits();
    CheckSecureStringSizeLimits();
    return CheckMemcpyOverlap() ? 0 : 1;
}
