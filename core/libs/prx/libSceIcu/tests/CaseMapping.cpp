#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t StringNotTerminated = -124;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s returned %lld, expected %lld\n", what, static_cast<long long>(actual), static_cast<long long>(expected));
    std::abort();
}

template <typename Exception, typename Function>
void ExpectThrow(Function function, const char* what) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    Expect(false, what);
}

using Mapper = std::int32_t (APS5_VABI*)(char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, std::int32_t*);

void ExpectMapping(Mapper map, const char* locale, const std::u16string& source, const std::u16string& expected, const char* what) {
    char16_t buffer[32] = {};
    std::int32_t error = Success;
    const auto length = map(buffer, 32, source.c_str(), -1, locale, &error);
    Expect(error == Success, what);
    ExpectInt(length, static_cast<std::int64_t>(expected.size()), what);
    Expect(std::u16string(buffer, length) == expected && buffer[length] == 0, what);
}

void TestMapping() {
    ExpectMapping(u_strToUpper_nid_postfix, "", u"straße", u"STRASSE", "u_strToUpper(straße)");
    ExpectMapping(u_strToUpper_nid_postfix, "tr", u"i", u"İ", "u_strToUpper(i, tr)");
    ExpectMapping(u_strToUpper_nid_postfix, "en", u"i", u"I", "u_strToUpper(i, en)");
    ExpectMapping(u_strToUpper_nid_postfix, "el", u"ά", u"Α", "u_strToUpper(U+03AC, el)");
    ExpectMapping(u_strToUpper_nid_postfix, "", u"ά", u"Ά", "u_strToUpper(U+03AC, root)");
    ExpectMapping(u_strToLower_nid_postfix, "az", u"I", u"ı", "u_strToLower(I, az)");
    ExpectMapping(u_strToLower_nid_postfix, "", u"İ", u"i̇", "u_strToLower(U+0130)");
    ExpectMapping(u_strToLower_nid_postfix, "lt", u"Ì", u"i̇̀", "u_strToLower(U+00CC, lt)");
    ExpectMapping(u_strToLower_nid_postfix, "", u"ΣAΣ", u"σaς", "u_strToLower(final sigma)");

    std::int32_t error = Success;
    ExpectInt(u_strToUpper_nid_postfix(nullptr, 0, u"straße", -1, "", &error), 7, "u_strToUpper preflight");
    ExpectInt(error, BufferOverflow, "u_strToUpper preflight error");
    char16_t exact[7];
    error = Success;
    ExpectInt(u_strToUpper_nid_postfix(exact, 7, u"straße", -1, "", &error), 7, "u_strToUpper exact capacity");
    ExpectInt(error, StringNotTerminated, "u_strToUpper exact capacity error");
    char16_t inPlace[8] = u"abc";
    error = Success;
    ExpectInt(u_strToUpper_nid_postfix(inPlace, 8, inPlace, -1, "", &error), 3, "u_strToUpper in place");
    Expect(error == Success && std::u16string(inPlace) == u"ABC", "u_strToUpper in place result");
    error = Success;
    ExpectInt(u_strToLower_nid_postfix(nullptr, 4, u"A", -1, "", &error), 0, "u_strToLower null destination");
    ExpectInt(error, IllegalArgument, "u_strToLower null destination error");
    error = BufferOverflow;
    ExpectInt(u_strToLower_nid_postfix(exact, 7, u"A", -1, nullptr, &error), 0, "u_strToLower failed input");
    ExpectInt(error, BufferOverflow, "u_strToLower keeps the input error");
    error = Success;
    ExpectInt(u_strToLower_nid_postfix(exact, 7, nullptr, -1, nullptr, &error), 0, "u_strToLower null source");
    ExpectInt(error, IllegalArgument, "u_strToLower null source error");
    error = Success;
    ExpectThrow<std::runtime_error>([&] { u_strToLower_nid_postfix(exact, 7, u"A", -1, nullptr, &error); }, "u_strToLower default locale");
    ExpectThrow<std::runtime_error>([&] { u_strToUpper_nid_postfix(exact, 7, u"a", -1, nullptr, &error); }, "u_strToUpper default locale");
    ExpectThrow<std::invalid_argument>([&] { u_strToUpper_nid_postfix(exact, 7, u"a", -1, "", nullptr); }, "u_strToUpper null error");

    char16_t folded[8] = {};
    error = Success;
    ExpectInt(u_strFoldCase_nid_postfix(folded, 8, u"ßI", -1, 0, &error), 3, "u_strFoldCase");
    Expect(error == Success && std::u16string(folded) == u"ssi", "u_strFoldCase result");
    error = Success;
    ExpectInt(u_strFoldCase_nid_postfix(folded, 8, u"I", 1, 1, &error), 1, "u_strFoldCase exclude special I");
    Expect(folded[0] == 0x131, "u_strFoldCase exclude special I result");
    ExpectThrow<std::invalid_argument>([&] { u_strFoldCase_nid_postfix(folded, 8, u"I", 1, 0, nullptr); }, "u_strFoldCase null error");
}

void TestComparison() {
    ExpectInt(u_strcasecmp_nid_postfix(u"HELLO", u"hello", 0), 0, "u_strcasecmp equal");
    Expect(u_strcasecmp_nid_postfix(u"a", u"B", 0) < 0, "u_strcasecmp less");
    ExpectInt(u_strcasecmp_nid_postfix(u"straße", u"STRASSE", 0), 0, "u_strcasecmp full folding");
    Expect(u_strcasecmp_nid_postfix(u"ı", u"I", 0) != 0, "u_strcasecmp dotless i");
    ExpectInt(u_strcasecmp_nid_postfix(u"ı", u"I", 1), 0, "u_strcasecmp exclude special I");
    ExpectInt(u_strncasecmp_nid_postfix(u"abcX", u"ABCY", 3, 0), 0, "u_strncasecmp prefix");
    Expect(u_strncasecmp_nid_postfix(u"abcX", u"ABCY", 4, 0) < 0, "u_strncasecmp differs");
    ExpectInt(u_strncasecmp_nid_postfix(u"ab", u"AB\0c", 4, 0), 0, "u_strncasecmp stops at NUL");
    ExpectInt(u_memcasecmp_nid_postfix(u"a\0B", u"A\0b", 3, 0), 0, "u_memcasecmp crosses NUL");
    Expect(u_memcasecmp_nid_postfix(u"a\0B", u"A\0c", 3, 0) < 0, "u_memcasecmp differs after NUL");
    ExpectInt(u_memcasecmp_nid_postfix(nullptr, nullptr, 0, 0), 0, "u_memcasecmp empty null");
    std::int32_t error = Success;
    ExpectInt(u_strCaseCompare_nid_postfix(u"Straße", -1, u"STRASSE", 7, 0, &error), 0, "u_strCaseCompare");
    ExpectInt(error, Success, "u_strCaseCompare error");
    ExpectInt(u_strCaseCompare_nid_postfix(nullptr, 1, u"a", 1, 0, &error), 0, "u_strCaseCompare null");
    ExpectInt(error, IllegalArgument, "u_strCaseCompare null error");
    ExpectInt(u_strCaseCompare_nid_postfix(u"a", 1, u"a", 1, 0, nullptr), 0, "u_strCaseCompare null error pointer");
    ExpectThrow<std::invalid_argument>([] { u_strcasecmp_nid_postfix(nullptr, u"a", 0); }, "u_strcasecmp null");
    ExpectThrow<std::invalid_argument>([] { u_memcasecmp_nid_postfix(u"a", u"a", -2, 0); }, "u_memcasecmp negative length");

    std::int32_t first = -1;
    std::int32_t second = -1;
    error = Success;
    u_caseInsensitivePrefixMatch_nid_postfix(u"Fust", -1, u"Fußball", -1, 0, &first, &second, &error);
    Expect(error == Success && first == 2 && second == 2, "u_caseInsensitivePrefixMatch partial fold");
    u_caseInsensitivePrefixMatch_nid_postfix(u"ABCd", 4, u"abcD", 4, 0, &first, &second, &error);
    Expect(first == 4 && second == 4, "u_caseInsensitivePrefixMatch full match");
    first = second = -1;
    error = BufferOverflow;
    u_caseInsensitivePrefixMatch_nid_postfix(u"a", 1, u"a", 1, 0, &first, nullptr, &error);
    Expect(first == -1 && error == BufferOverflow, "u_caseInsensitivePrefixMatch failed input");
    error = Success;
    u_caseInsensitivePrefixMatch_nid_postfix(u"a", 1, u"a", 1, 0, nullptr, nullptr, &error);
    ExpectThrow<std::invalid_argument>([&] { u_caseInsensitivePrefixMatch_nid_postfix(u"a", 1, u"a", 1, 0, &first, nullptr, &error); }, "u_caseInsensitivePrefixMatch missing second length");
    ExpectThrow<std::invalid_argument>([&] { u_caseInsensitivePrefixMatch_nid_postfix(u"a", 1, u"a", 1, 0, &first, &second, nullptr); }, "u_caseInsensitivePrefixMatch null error");
}

}

int main() {
    TestMapping();
    TestComparison();
    std::puts("ICU case mapping tests passed");
}
