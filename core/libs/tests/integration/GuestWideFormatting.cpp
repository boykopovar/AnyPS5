#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

extern "C" int APS5_VABI vswprintf_nid_postfix(char16_t*, std::size_t, const char16_t*, VaList*);
extern "C" int APS5_VABI snwprintf_s_nid_postfix(char16_t*, std::size_t, const char16_t*, ...);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

int APS5_VABI Format(char16_t* buffer, std::size_t size, const char16_t* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vswprintf_nid_postfix(buffer, size, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

std::string Units(std::u16string_view text) {
    std::string result;
    for (const char16_t unit : text) {
        char hex[8];
        std::snprintf(hex, sizeof(hex), "%04x ", static_cast<unsigned>(unit));
        result += hex;
    }
    return "[" + result + "]";
}

void RequireWide(const char16_t* actual, std::u16string_view expected, const std::string& message) {
    const std::u16string_view text(actual);
    if (text == expected) return;
    Testing::Fail(message + ": expected " + Units(expected) + ", got " + Units(text));
}

void RequireUnit(char16_t actual, char16_t expected, const std::string& message) {
    RequireEqual(static_cast<int>(actual), static_cast<int>(expected), message);
}

const Case completeOutput{"SnwprintfS_FittingOutput_WritesAllUnits", [] {
    char16_t buffer[16];
    buffer[0] = u'x';
    RequireEqual(snwprintf_s_nid_postfix(buffer, 16, u"%d-%ls-%s", 42, u"ab", "\xc3\xa9"), 7, "length");
    RequireWide(buffer, u"42-ab-é", "output");
}};

const Case truncatedOutput{"SnwprintfS_SmallBuffer_TruncatesAndReturnsFullLength", [] {
    char16_t buffer[16];
    buffer[5] = u'x';
    RequireEqual(snwprintf_s_nid_postfix(buffer, 5, u"%d-%ls", 42, u"abcd"), 7, "length");
    RequireWide(buffer, u"42-a", "output");
    RequireUnit(buffer[5], u'x', "unit past the buffer size");
}};

const Case oneUnitBuffer{"SnwprintfS_OneUnitBuffer_WritesOnlyTerminator", [] {
    char16_t buffer[16];
    buffer[0] = u'x';
    buffer[1] = u'y';
    RequireEqual(snwprintf_s_nid_postfix(buffer, 1, u"%x", 0xabcu), 3, "length");
    RequireUnit(buffer[0], 0, "terminator");
    RequireUnit(buffer[1], u'y', "unit past the buffer size");
}};

const Case emptyFormat{"SnwprintfS_EmptyFormat_WritesEmptyString", [] {
    char16_t buffer[16];
    buffer[0] = u'x';
    RequireEqual(snwprintf_s_nid_postfix(buffer, 16, u""), 0, "length");
    RequireUnit(buffer[0], 0, "terminator");
}};

const Case nullFormat{"SnwprintfS_NullFormat_FailsAndClearsBuffer", [] {
    char16_t buffer[16];
    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, nullptr) < 0, "result is negative");
    RequireUnit(buffer[0], 0, "terminator");
}};

const Case nullBuffer{"SnwprintfS_NullBuffer_Fails", [] {
    Require(snwprintf_s_nid_postfix(nullptr, 16, u"a") < 0, "result is negative");
}};

const Case zeroSize{"SnwprintfS_ZeroSize_FailsWithoutWriting", [] {
    char16_t buffer[16];
    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 0, u"a") < 0, "result is negative");
    RequireUnit(buffer[0], u'x', "buffer untouched");
}};

const Case hugeSize{"SnwprintfS_SizeAboveRsizeMax_FailsWithoutWriting", [] {
    constexpr std::size_t rsizeMax = SIZE_MAX >> 1;
    char16_t buffer[16];
    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, rsizeMax + 1, u"a") < 0, "result is negative");
    RequireUnit(buffer[0], u'x', "buffer untouched");
}};

const Case countRejected{"SnwprintfS_CountConversion_FailsAndDoesNotStore", [] {
    char16_t buffer[16];
    int written = -7;
    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, u"ab%n", &written) < 0, "result is negative");
    RequireUnit(buffer[0], 0, "terminator");
    RequireEqual(written, -7, "count untouched");
}};

const Case nullWideArgument{"SnwprintfS_NullWideStringArgument_FailsAndClearsBuffer", [] {
    const char16_t* const nullWide = nullptr;
    char16_t buffer[16];
    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, u"a%ls", nullWide) < 0, "result is negative");
    RequireUnit(buffer[0], 0, "terminator");
}};

const Case nullNarrowArgument{"SnwprintfS_NullNarrowStringArgument_FailsAndClearsBuffer", [] {
    const char* const nullNarrow = nullptr;
    char16_t buffer[16];
    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, u"a%s", nullNarrow) < 0, "result is negative");
    RequireUnit(buffer[0], 0, "terminator");
}};

const Case vswprintfNullNarrow{"Vswprintf_NullNarrowStringArgument_PrintsNullMarker", [] {
    const char* const nullNarrow = nullptr;
    char16_t buffer[16];
    RequireEqual(Format(buffer, 16, u"%s", nullNarrow), 6, "length");
    RequireWide(buffer, u"(null)", "output");
}};

const Case countMiddle{"Vswprintf_CountInMiddle_StoresUnitsWrittenSoFar", [] {
    char16_t buffer[16];
    int count = -7;
    RequireEqual(Format(buffer, 16, u"ab%ncd", &count), 4, "length");
    RequireWide(buffer, u"abcd", "output");
    RequireEqual(count, 2, "count");
}};

const Case countUtf16{"Vswprintf_CountAfterSupplementaryCharacter_CountsUtf16Units", [] {
    char16_t buffer[16];
    int count = -7;
    RequireEqual(Format(buffer, 16, u"%s%n!", "\xf0\x9f\x98\x80", &count), 3, "length");
    RequireEqual(count, 2, "count");
}};

const Case countStart{"Vswprintf_CountAtStart_StoresZero", [] {
    char16_t buffer[16];
    int count = -7;
    RequireEqual(Format(buffer, 16, u"%n", &count), 0, "length");
    RequireUnit(buffer[0], 0, "terminator");
    RequireEqual(count, 0, "count");
}};

const Case countChar{"Vswprintf_HhCount_StoresOneByte", [] {
    char16_t buffer[16];
    unsigned char bytes[2] = {0xaa, 0xaa};
    RequireEqual(Format(buffer, 16, u"%3d%hhn", 5, bytes), 3, "length");
    RequireEqual(static_cast<int>(bytes[0]), 3, "count");
    RequireEqual(static_cast<int>(bytes[1]), 0xaa, "next byte untouched");
}};

const Case countShort{"Vswprintf_HCount_StoresOneShort", [] {
    char16_t buffer[16];
    unsigned short halves[2] = {0xaaaa, 0xaaaa};
    RequireEqual(Format(buffer, 16, u"%5d%hn", 5, halves), 5, "length");
    RequireEqual(static_cast<int>(halves[0]), 5, "count");
    RequireEqual(static_cast<int>(halves[1]), 0xaaaa, "next short untouched");
}};

const Case countLongLong{"Vswprintf_LlCount_StoresLongLong", [] {
    char16_t buffer[16];
    long long wide = -1;
    RequireEqual(Format(buffer, 16, u"%4d%lln", 5, &wide), 4, "length");
    RequireEqual(wide, 4LL, "count");
}};

const Case countLong{"Vswprintf_LCount_StoresSixtyFourBits", [] {
    char16_t buffer[16];
    long long wide = -1;
    RequireEqual(Format(buffer, 16, u"%2d%ln", 5, &wide), 2, "length");
    RequireEqual(wide, 2LL, "count");
}};

const Case countWidth{"Vswprintf_CountWithWidth_FailsAndDoesNotStore", [] {
    char16_t buffer[16];
    int count = -7;
    Require(Format(buffer, 16, u"a%5n", &count) < 0, "result is negative");
    RequireEqual(count, -7, "count untouched");
}};

const Case countLongDouble{"Vswprintf_CapitalLCount_FailsAndDoesNotStore", [] {
    char16_t buffer[16];
    int count = -7;
    Require(Format(buffer, 16, u"a%Ln", &count) < 0, "result is negative");
    RequireEqual(count, -7, "count untouched");
}};

const Case countNull{"Vswprintf_NullCountArgument_Fails", [] {
    char16_t buffer[16];
    const int* const nullCount = nullptr;
    Require(Format(buffer, 16, u"a%n", nullCount) < 0, "result is negative");
}};

const Case precision{"Vswprintf_NarrowStringPrecisionAndWidth_CountUtf16Units", [] {
    struct PrecisionCase {
        const char16_t* format;
        const char* input;
        const char16_t* expected;
        const char* label;
    };
    const PrecisionCase cases[] = {
        {u"%.2s", "\xc3\xa9\xc3\xa8", u"éè", "%.2s of two two-byte characters"},
        {u"%.1s", "\xc3\xa9\xc3\xa8", u"é", "%.1s of two two-byte characters"},
        {u"%.0s", "\xc3\xa9", u"", "%.0s"},
        {u"%.3s", "\xc3\xa9", u"é", "%.3s of one two-byte character"},
        {u"%s", "\xc3\xa9\xc3\xa8", u"éè", "%s of two two-byte characters"},
        {u"%.2s", "abcd", u"ab", "%.2s of ascii"},
        {u"%4.2s", "\xc3\xa9\xc3\xa8", u"  éè", "%4.2s"},
        {u"%-4.2s", "\xc3\xa9\xc3\xa8", u"éè  ", "%-4.2s"},
        {u"%.2s", "\xf0\x9f\x98\x80x", u"\U0001f600", "%.2s of a surrogate pair"},
        {u"%.1s", "\xf0\x9f\x98\x80x", u"", "%.1s of a surrogate pair"},
        {u"%.3s", "\xf0\x9f\x98\x80x", u"\U0001f600x", "%.3s of a surrogate pair and ascii"},
    };
    for (const auto& test : cases) {
        char16_t buffer[32]{};
        const std::u16string_view expected(test.expected);
        RequireEqual(Format(buffer, 32, test.format, test.input), static_cast<int>(expected.size()),
            std::string(test.label) + ": length");
        RequireWide(buffer, expected, std::string(test.label) + ": output");
    }
}};

} // namespace
