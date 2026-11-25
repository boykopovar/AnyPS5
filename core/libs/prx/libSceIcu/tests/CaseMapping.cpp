#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t StringNotTerminated = -124;
constexpr std::uint32_t ExcludeSpecialI = 1;
constexpr std::uint32_t EditsNoReset = 0x2000;
constexpr std::uint32_t OmitUnchangedText = 0x4000;

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

using LocaleMapper = std::int32_t (*)(const char*, std::uint32_t, const char16_t*, std::int32_t, char16_t*, std::int32_t, void*, std::int32_t*);

std::int32_t ToLower(const char* locale, std::uint32_t options, const char16_t* source, std::int32_t length, char16_t* destination,
                     std::int32_t capacity, void* edits, std::int32_t* error) {
    return _ZN6icu_687CaseMap7toLowerEPKcjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(locale, options, source, length, destination,
                                                                                        capacity, edits, error);
}

std::int32_t ToUpper(const char* locale, std::uint32_t options, const char16_t* source, std::int32_t length, char16_t* destination,
                     std::int32_t capacity, void* edits, std::int32_t* error) {
    return _ZN6icu_687CaseMap7toUpperEPKcjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(locale, options, source, length, destination,
                                                                                        capacity, edits, error);
}

std::int32_t Fold(const char*, std::uint32_t options, const char16_t* source, std::int32_t length, char16_t* destination,
                  std::int32_t capacity, void* edits, std::int32_t* error) {
    return _ZN6icu_687CaseMap4foldEjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(options, source, length, destination, capacity, edits,
                                                                                  error);
}

constexpr LocaleMapper Mappers[] = {ToLower, ToUpper, Fold};

void ExpectCaseMap(LocaleMapper map, const char* locale, std::uint32_t options, const std::u16string& source, std::int32_t length,
                   const std::u16string& expected, const char* what) {
    char16_t buffer[32];
    std::fill(std::begin(buffer), std::end(buffer), u'ꪪ');
    std::int32_t error = Success;
    const auto result = map(locale, options, source.data(), length, buffer, 32, nullptr, &error);
    ExpectInt(error, Success, what);
    ExpectInt(result, static_cast<std::int64_t>(expected.size()), what);
    Expect(std::u16string(buffer, result) == expected && buffer[result] == 0 && buffer[result + 1] == u'ꪪ', what);
}

void ExpectCall(LocaleMapper map, const char16_t* source, std::int32_t length, char16_t* destination, std::int32_t capacity, void* edits,
                std::int32_t errorIn, std::int64_t result, std::int32_t errorOut, const char* what) {
    std::int32_t error = errorIn;
    ExpectInt(map("", 0, source, length, destination, capacity, edits, &error), result, what);
    ExpectInt(error, errorOut, what);
}

template <typename Function>
void ExpectNotImplemented(Function function, const char* what) {
    const auto before = NotImplementedCalls_nid_no_patch();
    ExpectThrow<std::runtime_error>(function, what);
    ExpectInt(static_cast<std::int64_t>(NotImplementedCalls_nid_no_patch() - before), 1, what);
}

void TestCaseMapResults() {
    ExpectCaseMap(ToLower, "tr", 0, u"Iİ", -1, u"ıi", "CaseMap::toLower(tr)");
    ExpectCaseMap(ToLower, "tr_TR", 0, u"I", -1, u"ı", "CaseMap::toLower(tr_TR)");
    ExpectCaseMap(ToLower, "en", 0, u"Iİ", -1, u"ii̇", "CaseMap::toLower(en)");
    ExpectCaseMap(ToUpper, "az", 0, u"i", -1, u"İ", "CaseMap::toUpper(az)");
    ExpectCaseMap(ToLower, "lt", 0, u"Ì", -1, u"i̇̀", "CaseMap::toLower(lt)");
    ExpectCaseMap(ToUpper, "el", 0, u"ά", -1, u"Α", "CaseMap::toUpper(el)");
    ExpectCaseMap(ToUpper, "", 0, u"ά", -1, u"Ά", "CaseMap::toUpper(root)");
    ExpectCaseMap(ToUpper, "hy", 0, u"և", -1, u"ԵՎ", "CaseMap::toUpper(hy)");
    ExpectCaseMap(ToUpper, "x-garbage", 0, u"և", -1, u"ԵՒ", "CaseMap::toUpper(unknown locale)");
    ExpectCaseMap(ToLower, "", 0, u"ΣAΣ", -1, u"σaς", "CaseMap::toLower(final sigma)");
    ExpectCaseMap(ToUpper, "", 0, u"straße", -1, u"STRASSE", "CaseMap::toUpper(sharp s)");
    ExpectCaseMap(ToLower, "", 0, std::u16string(u"A\0B", 3), 3, std::u16string(u"a\0b", 3), "CaseMap::toLower(embedded NUL)");
    ExpectCaseMap(ToLower, "", 0, u"A", 0, u"", "CaseMap::toLower(empty)");
    ExpectCaseMap(ToUpper, "", OmitUnchangedText, u"aBc", -1, u"AC", "CaseMap::toUpper(omit unchanged text)");
    ExpectCaseMap(ToUpper, "", 0x80000000u, u"aBc", -1, u"ABC", "CaseMap::toUpper(unknown option)");
    ExpectCaseMap(Fold, "tr", 0, u"ßİ", -1, u"ssi̇", "CaseMap::fold");
    ExpectCaseMap(Fold, "", ExcludeSpecialI, u"Iİ", -1, u"ıi", "CaseMap::fold(exclude special I)");
    const std::u16string surrogates{char16_t(0xdc00), u'A', char16_t(0xd800), char16_t(0xd800), char16_t(0xdc00), u'A', char16_t(0xdbff)};
    const std::u16string foldedSurrogates{char16_t(0xdc00), u'a', char16_t(0xd800), char16_t(0xd800), char16_t(0xdc00), u'a', char16_t(0xdbff)};
    ExpectCaseMap(Fold, "", 0, surrogates, -1, foldedSurrogates, "CaseMap::fold(unpaired surrogates)");
    ExpectCaseMap(Fold, "", 0, u"\U00010400", -1, u"\U00010428", "CaseMap::fold(supplementary)");

    std::int32_t error = Success;
    uloc_setDefault_nid_postfix("tr", &error);
    ExpectCaseMap(ToUpper, nullptr, 0, u"i", -1, u"İ", "CaseMap::toUpper(default tr)");
    ExpectCaseMap(ToLower, "", 0, u"I", -1, u"i", "CaseMap::toLower(empty locale under a tr default)");
    uloc_setDefault_nid_postfix("lt", &error);
    ExpectCaseMap(ToLower, nullptr, 0, u"Ì", -1, u"i̇̀", "CaseMap::toLower(default lt)");
    ExpectInt(error, Success, "uloc_setDefault");
}

void TestCaseMapBuffers() {
    char16_t buffer[8];
    for (auto map : Mappers) {
        ExpectCall(map, u"aB", -1, nullptr, 0, nullptr, Success, 2, BufferOverflow, "CaseMap preflight");
        ExpectCall(map, u"aB", -1, buffer, 1, nullptr, Success, 2, BufferOverflow, "CaseMap short buffer");
        ExpectCall(map, u"aB", -1, buffer, 2, nullptr, Success, 2, StringNotTerminated, "CaseMap exact capacity");
        ExpectCall(map, u"aB", -1, buffer, 3, nullptr, StringNotTerminated, 2, Success, "CaseMap clears a warning");
        ExpectCall(map, u"aB", -1, buffer, -1, nullptr, Success, 0, IllegalArgument, "CaseMap negative capacity");
        ExpectCall(map, u"aB", -1, nullptr, 4, nullptr, Success, 0, IllegalArgument, "CaseMap null destination");
        ExpectCall(map, nullptr, -1, buffer, 4, nullptr, Success, 0, IllegalArgument, "CaseMap null source");
        ExpectCall(map, u"aB", -2, buffer, 4, nullptr, Success, 0, IllegalArgument, "CaseMap length below -1");
        buffer[0] = u'x';
        ExpectCall(map, u"aB", -1, buffer, 4, nullptr, BufferOverflow, 0, BufferOverflow, "CaseMap failed input");
        ExpectCall(map, u"aB", -1, buffer, 4, nullptr, IllegalArgument, 0, IllegalArgument, "CaseMap failed input");
        Expect(buffer[0] == u'x', "CaseMap failed input writes nothing");
        ExpectThrow<std::invalid_argument>([&] { map("", 0, u"a", -1, buffer, 4, nullptr, nullptr); }, "CaseMap null error");
    }
    ExpectCall(ToUpper, u"straße", -1, buffer, 3, nullptr, Success, 7, BufferOverflow, "CaseMap::toUpper growth overflow");
    ExpectCall(ToUpper, u"straße", -1, buffer, 7, nullptr, Success, 7, StringNotTerminated, "CaseMap::toUpper growth exact");
}

void TestCaseMapOverlap() {
    for (auto map : Mappers) {
        char16_t text[8] = u"abcd";
        ExpectCall(map, text, -1, text, 8, nullptr, Success, 0, IllegalArgument, "CaseMap in place");
        ExpectCall(map, text + 2, 2, text, 3, nullptr, Success, 0, IllegalArgument, "CaseMap destination reaches the source");
        ExpectCall(map, text, 2, text + 1, 2, nullptr, Success, 0, IllegalArgument, "CaseMap destination inside the source");
        ExpectCall(map, text, -1, text + 3, 2, nullptr, Success, 0, IllegalArgument, "CaseMap destination inside the terminated source");
        Expect(std::u16string(text) == u"abcd", "CaseMap overlap writes nothing");
    }
    char16_t text[8] = u"abcd";
    ExpectCall(ToUpper, text + 2, 2, text, 2, nullptr, Success, 2, StringNotTerminated, "CaseMap destination before the source");
    ExpectCall(ToUpper, text, 2, text + 2, 2, nullptr, Success, 2, StringNotTerminated, "CaseMap destination after the source");
    Expect(std::u16string(text) == u"CDCD", "CaseMap adjacent buffers");
}

void TestCaseMapEdits() {
    alignas(8) unsigned char edits[128] = {};
    char16_t buffer[8] = {};
    char16_t text[8] = u"abcd";
    for (auto map : Mappers) {
        ExpectCall(map, u"aB", -1, buffer, 4, edits, BufferOverflow, 0, BufferOverflow, "CaseMap edits with failed input");
        ExpectCall(map, u"aB", -1, buffer, -1, edits, Success, 0, IllegalArgument, "CaseMap edits with negative capacity");
        ExpectCall(map, u"aB", -1, nullptr, 4, edits, Success, 0, IllegalArgument, "CaseMap edits with null destination");
        ExpectCall(map, nullptr, 0, buffer, 4, edits, Success, 0, IllegalArgument, "CaseMap edits with null source");
        ExpectCall(map, u"aB", -2, buffer, 4, edits, Success, 0, IllegalArgument, "CaseMap edits with length below -1");
        ExpectCall(map, text, -1, text + 3, 2, edits, Success, 0, IllegalArgument, "CaseMap edits with overlap");
        ExpectCall(map, text, 2, text + 1, 2, edits, Success, 0, IllegalArgument, "CaseMap edits with overlap");
        ExpectNotImplemented([&] {
            std::int32_t error = Success;
            map("", 0, u"aB", -1, buffer, 4, edits, &error);
        }, "CaseMap edits");
        ExpectNotImplemented([&] {
            std::int32_t error = Success;
            map("", EditsNoReset, u"aB", -1, buffer, 4, edits, &error);
        }, "CaseMap edits without reset");
        ExpectNotImplemented([&] {
            std::int32_t error = StringNotTerminated;
            map("", 0, u"aB", 2, nullptr, 0, edits, &error);
        }, "CaseMap edits preflight");
        ExpectNotImplemented([&] {
            std::int32_t error = Success;
            map("", 0, text + 2, 2, text, 2, edits, &error);
        }, "CaseMap edits with adjacent buffers");
        ExpectNotImplemented([&] {
            std::int32_t error = Success;
            map("", 0, text, -1, text + 4, 2, edits, &error);
        }, "CaseMap edits after the terminated source");
    }
    Expect(std::u16string(text) == u"abcd" && edits[0] == 0, "CaseMap edits write nothing");
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

void TestUtf8CaseMap() {
    std::int32_t error = Success;
    auto* caseMap = ucasemap_open_nid_postfix("tr", 0, &error);
    Expect(caseMap != nullptr, "ucasemap_open");
    ExpectInt(error, Success, "ucasemap_open status");
    char buffer[64] = {};
    ExpectInt(ucasemap_utf8ToUpper_nid_postfix(caseMap, buffer, 64, "i stra\xC3\x9F" "e", -1, &error), 10,
              "ucasemap_utf8ToUpper");
    ExpectInt(error, Success, "ucasemap_utf8ToUpper status");
    Expect(std::string(buffer) == "\xC4\xB0 STRASSE", "ucasemap_utf8ToUpper result");
    ExpectInt(ucasemap_utf8FoldCase_nid_postfix(caseMap, buffer, 64, "Stra\xC3\x9F" "e", -1, &error), 7,
              "ucasemap_utf8FoldCase");
    ExpectInt(error, Success, "ucasemap_utf8FoldCase status");
    Expect(std::string(buffer) == "strasse", "ucasemap_utf8FoldCase result");
    Expect(std::string(ucasemap_getLocale_nid_postfix(caseMap)) == "tr", "ucasemap_getLocale");
    ExpectInt(ucasemap_getOptions_nid_postfix(caseMap), 0, "ucasemap_getOptions");
    error = Success;
    ExpectInt(ucasemap_utf8ToLower_nid_postfix(caseMap, buffer, 64, "I", -1, &error), 2,
              "ucasemap_utf8ToLower(tr)");
    Expect(error == Success && std::string(buffer) == "\xC4\xB1", "ucasemap_utf8ToLower(tr) result");
    ucasemap_setLocale_nid_postfix(caseMap, "en", &error);
    Expect(error == Success && std::string(ucasemap_getLocale_nid_postfix(caseMap)) == "en", "ucasemap_setLocale");
    ExpectInt(ucasemap_utf8ToLower_nid_postfix(caseMap, buffer, 64, "I", -1, &error), 1,
              "ucasemap_utf8ToLower(en)");
    Expect(std::string(buffer) == "i", "ucasemap_utf8ToLower(en) result");
    ucasemap_setOptions_nid_postfix(caseMap, ExcludeSpecialI, &error);
    ExpectInt(ucasemap_getOptions_nid_postfix(caseMap), ExcludeSpecialI, "ucasemap_setOptions");
    ExpectThrow<std::invalid_argument>([] { ucasemap_getLocale_nid_postfix(nullptr); }, "ucasemap_getLocale null");
    ExpectThrow<std::invalid_argument>([] { ucasemap_getOptions_nid_postfix(nullptr); }, "ucasemap_getOptions null");
    ExpectThrow<std::invalid_argument>([] { ucasemap_setLocale_nid_postfix(nullptr, "en", nullptr); },
                                       "ucasemap_setLocale null");
    ExpectThrow<std::invalid_argument>([] { ucasemap_setOptions_nid_postfix(nullptr, 0, nullptr); },
                                       "ucasemap_setOptions null");
    ExpectThrow<std::invalid_argument>([] { ucasemap_utf8ToLower_nid_postfix(nullptr, nullptr, 0, "", 0, nullptr); },
                                       "ucasemap_utf8ToLower null");
    ucasemap_close_nid_postfix(caseMap);
    ucasemap_close_nid_postfix(nullptr);
    ExpectThrow<std::invalid_argument>([] { ucasemap_open_nid_postfix("", 0, nullptr); }, "ucasemap_open null status");
}

void TestReverseBidi() {
    char16_t reversed[16] = {};
    std::int32_t error = Success;
    ExpectInt(ubidi_writeReverse_nid_postfix(u"abc \u05d0\u05d1", 6, reversed, 16, 0, &error), 6,
              "ubidi_writeReverse");
    ExpectInt(error, Success, "ubidi_writeReverse status");
    Expect(std::u16string(reversed) == u"\u05d1\u05d0 cba", "ubidi_writeReverse result");
    error = Success;
    ExpectInt(ubidi_writeReverse_nid_postfix(u"abc", 3, nullptr, 0, 0, &error), 3,
              "ubidi_writeReverse preflight");
    ExpectInt(error, BufferOverflow, "ubidi_writeReverse preflight status");
    ExpectThrow<std::invalid_argument>([] { ubidi_writeReverse_nid_postfix(u"a", 1, nullptr, 0, 0, nullptr); },
                                       "ubidi_writeReverse null status");
}

}

int main() {
    TestMapping();
    TestCaseMapResults();
    TestCaseMapBuffers();
    TestCaseMapOverlap();
    TestCaseMapEdits();
    TestComparison();
    TestUtf8CaseMap();
    TestReverseBidi();
    std::puts("ICU case mapping tests passed");
}
