#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

extern "C" {
int APS5_VABI snprintf_nid_postfix(char*, size_t, const char*, ...);
int APS5_VABI sprintf_nid_postfix(char*, const char*, ...);
int APS5_VABI printf_nid_postfix(const char*, ...);
int APS5_VABI libc_printf_nid_postfix(const char*, ...);
int APS5_VABI sscanf_nid_postfix(const char*, const char*, ...);
int APS5_VABI vsnprintf_nid_postfix(char*, size_t, const char*, VaList*);
int APS5_VABI vprintf_nid_postfix(const char*, VaList*);
}

namespace {

using namespace std::string_view_literals;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

int APS5_VABI FormatList(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const VaList original = list;
    const int result = vsnprintf_nid_postfix(buffer, size, format, &list);
    const bool unchanged = std::memcmp(&list, &original, sizeof(list)) == 0;
    __builtin_sysv_va_end(args);
    Require(unchanged, std::string("vsnprintf left the caller va_list untouched for ") + format);
    return result;
}

int APS5_VABI PrintList(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = vprintf_nid_postfix(format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

std::string_view Text(const char* buffer) {
    return std::string_view(buffer);
}

const Case widePrecision{"Snprintf_WideStringPrecision_StopsOnCompleteUtf8Sequences", [] {
    const char16_t input[] = u"Aé€\U0001f600Z";
    const char* expected[] = {"", "A", "A", "A\xc3\xa9", "A\xc3\xa9", "A\xc3\xa9", "A\xc3\xa9\xe2\x82\xac",
        "A\xc3\xa9\xe2\x82\xac", "A\xc3\xa9\xe2\x82\xac", "A\xc3\xa9\xe2\x82\xac",
        "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80Z"};
    for (int precision = 0; precision < 12; ++precision) {
        char output[32];
        std::memset(output, '!', sizeof(output));
        const int count = snprintf_nid_postfix(output, sizeof(output), "%.*ls", precision, input);
        const auto expectedSize = std::strlen(expected[precision]);
        const std::string label = "wide precision " + std::to_string(precision);
        RequireEqual(count, static_cast<int>(expectedSize), label + " count");
        RequireEqual(Text(output), std::string_view(expected[precision]), label + " text");
        RequireEqual(output[expectedSize + 1], '!', label + " guard byte");
    }
}};

const Case widePrecisionWidth{"Snprintf_WideStringPrecisionWithWidth_PadsCompleteCharacters", [] {
    char output[32];
    RequireEqual(snprintf_nid_postfix(output, sizeof(output), "[%5.1ls]", u"é"), 7, "[%5.1ls] count");
    RequireEqual(Text(output), "[     ]"sv, "[%5.1ls] text");
    RequireEqual(snprintf_nid_postfix(output, sizeof(output), "[%-5.2ls]", u"é"), 7, "[%-5.2ls] count");
    RequireEqual(Text(output), "[\xc3\xa9   ]"sv, "[%-5.2ls] text");
}};

const Case widePrecisionTooShort{"Vsnprintf_PrecisionShorterThanCharacter_WritesNothing", [] {
    char output[32];
    RequireEqual(FormatList(output, sizeof(output), "%.3ls", u"\U0001f600"), 0, "%.3ls count");
    RequireEqual(output[0], '\0', "%.3ls terminator");
}};

const Case wideNegativePrecision{"Vsnprintf_NegativeWidePrecision_IsUnlimited", [] {
    char output[32];
    RequireEqual(FormatList(output, sizeof(output), "%.*ls:%d", -1, u"é", 7), 4, "count");
    RequireEqual(Text(output), "\xc3\xa9:7"sv, "text");
}};

const Case wideBoundedArray{"Snprintf_WidePrecisionOnUnterminatedArray_ReadsOnlyPrecision", [] {
    char output[32];
    const char16_t bounded[] = {u'A', u'B'};
    RequireEqual(snprintf_nid_postfix(output, sizeof(output), "%.2ls", bounded), 2, "count");
    RequireEqual(Text(output), "AB"sv, "text");
}};

const Case wideNullBuffer{"Snprintf_WidePrecisionIntoNullBuffer_CountsCompleteCharacters", [] {
    RequireEqual(snprintf_nid_postfix(nullptr, 0, "%.1ls", u"é"), 0, "count");
}};

const Case narrowPrecision{"Snprintf_NarrowStringPrecision_CountsBytes", [] {
    char output[32];
    RequireEqual(snprintf_nid_postfix(output, sizeof(output), "%.1s", "\xc3\xa9"), 1, "count");
    RequireEqual(static_cast<unsigned char>(output[0]), static_cast<unsigned char>(0xc3), "first byte");
    RequireEqual(output[1], '\0', "terminator");
}};

const Case wideConversions{"Vsnprintf_WideConversionSpecifiers_EncodeUtf8", [] {
    char output[32];
    RequireEqual(FormatList(output, sizeof(output), "%S:%lc:%C:%d", u"é", 0x20ac, 0x41, 7), 10, "count");
    RequireEqual(Text(output), "\xc3\xa9:\xe2\x82\xac:A:7"sv, "text");
}};

const Case upperSPrecision{"Snprintf_UpperSPrecision_StopsOnCompleteUtf8Sequences", [] {
    char output[32];
    RequireEqual(snprintf_nid_postfix(output, sizeof(output), "%.*S", 3, u"é€"), 2, "count");
    RequireEqual(Text(output), "\xc3\xa9"sv, "text");
}};

const Case wideNullString{"Snprintf_NullWideString_PrintsNull", [] {
    char output[32];
    RequireEqual(snprintf_nid_postfix(output, sizeof(output), "[%ls]", static_cast<const char16_t*>(nullptr)), 8, "count");
    RequireEqual(Text(output), "[(null)]"sv, "text");
}};

const Case wideZeroCharacter{"Snprintf_ZeroWideCharacter_WritesEmbeddedNul", [] {
    char output[32];
    RequireEqual(snprintf_nid_postfix(output, sizeof(output), "a%lcb%C", 0, 0), 4, "count");
    RequireEqual(std::string_view(output, 5), "a\0b\0\0"sv, "bytes");
}};

#ifndef _WIN32
const Case positionalArguments{"Vsnprintf_PositionalArguments_SelectArgumentsByIndex", [] {
    char output[32];
    RequireEqual(FormatList(output, sizeof(output), "%%ls:%2$d:%1$d", 3, 7), 7, "count");
    RequireEqual(Text(output), "%ls:7:3"sv, "text");
}};
#endif

const Case wideCapacity{"Vsnprintf_WideStringWithSmallCapacity_TruncatesAndCountsFullLength", [] {
    for (std::size_t capacity = 0; capacity <= 6; ++capacity) {
        char output[32];
        std::memset(output, '!', sizeof(output));
        int count = -1;
        const std::string label = "capacity " + std::to_string(capacity);
        RequireEqual(FormatList(capacity ? output : nullptr, capacity, "%ls%n", u"AéZ", &count), 4, label + " result");
        RequireEqual(count, 4, label + " %n");
        const auto copied = capacity == 0 ? 0 : std::min<std::size_t>(capacity - 1, 4);
        RequireEqual(std::string_view(output, copied), std::string_view("A\xc3\xa9Z", copied), label + " copied bytes");
        RequireEqual(output[capacity], '!', label + " guard byte");
        if (capacity) RequireEqual(output[copied], '\0', label + " terminator");
    }
}};

const Case vsnprintfSingleArgument{"Vsnprintf_SingleInteger_FormatsMessage", [] {
    char buffer[1024];
    RequireEqual(FormatList(buffer, sizeof(buffer), "Mount requested: %d", 0), 18, "count");
    RequireEqual(Text(buffer), "Mount requested: 0"sv, "text");
}};

const Case repeatedStackArguments{"Formatting_RepeatedCallsWithStackArguments_StayCorrect", [] {
    char buffer[1024];
    for (int index = 0; index < 10000; ++index) {
        const std::string label = "iteration " + std::to_string(index);
        snprintf_nid_postfix(buffer, sizeof(buffer), "%d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8);
        RequireEqual(Text(buffer), "1 2 3 4 5 6 7 8"sv, label + " integers");
        FormatList(buffer, sizeof(buffer), "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %d", 1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 11);
        RequireEqual(Text(buffer), "1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0 11"sv, label + " doubles");
        snprintf_nid_postfix(buffer, sizeof(buffer), "%ld %lu %lld %zu %td %jd", -4294967297LL, 4294967297ULL, -5LL, 7ULL, -8LL, 9LL);
        RequireEqual(Text(buffer), "-4294967297 4294967297 -5 7 -8 9"sv, label + " length modifiers");
        snprintf_nid_postfix(buffer, sizeof(buffer), "%s:%*.*f:%d", "test", -8, 2, 1.25, 7);
        RequireEqual(Text(buffer), "test:1.25    :7"sv, label + " star width");
        FormatList(buffer, sizeof(buffer), "%d %d %d %d %d %d %d %.3Lf %.1f", 1, 2, 3, 4, 5, 6, 7, 1.125L, 2.5);
        RequireEqual(Text(buffer), "1 2 3 4 5 6 7 1.125 2.5"sv, label + " long double");
    }
}};

const Case tenDoubles{"Snprintf_TenDoubles_FormatsAll", [] {
    char buffer[1024];
    snprintf_nid_postfix(buffer, sizeof(buffer), "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f", 1., 2., 3., 4., 5., 6., 7., 8., 9., 10.);
    RequireEqual(Text(buffer), "1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0"sv, "text");
}};

const Case longDoubleFirst{"Snprintf_LongDoubleBeforeOtherArguments_FormatsAll", [] {
    char buffer[1024];
    snprintf_nid_postfix(buffer, sizeof(buffer), "%.3Lf %d %.1f", 1.125L, 7, 2.5);
    RequireEqual(Text(buffer), "1.125 7 2.5"sv, "text");
}};

const Case matchesHost{"Snprintf_AlternateHexExponentAndPointer_MatchHostFormatting", [] {
    char buffer[1024];
    char expected[1024];
    std::snprintf(expected, sizeof(expected), "%#08x %.3e %a %g %p", 42u, 1.25, 1.25, 1.25, static_cast<void*>(buffer));
    snprintf_nid_postfix(buffer, sizeof(buffer), "%#08x %.3e %a %g %p", 42u, 1.25, 1.25, 1.25, static_cast<void*>(buffer));
    RequireEqual(Text(buffer), Text(expected), "text");
}};

const Case negativeStringPrecision{"Snprintf_NegativeStringPrecision_IsUnlimited", [] {
    char buffer[1024];
    snprintf_nid_postfix(buffer, sizeof(buffer), "%.*s", -1, "unlimited");
    RequireEqual(Text(buffer), "unlimited"sv, "text");
}};

const Case truncation{"Snprintf_SmallBuffer_TruncatesAndReturnsFullLength", [] {
    char buffer[1024];
    buffer[5] = '!';
    RequireEqual(snprintf_nid_postfix(buffer, 5, "%s", "abcdef"), 6, "count");
    RequireEqual(Text(buffer), "abcd"sv, "text");
    RequireEqual(buffer[5], '!', "guard byte");
}};

const Case nullBuffer{"Snprintf_NullBuffer_ReturnsRequiredLength", [] {
    RequireEqual(snprintf_nid_postfix(nullptr, 0, "%s:%d", "abcdef", 123), 10, "count");
}};

const Case nullString{"Snprintf_NullString_PrintsNull", [] {
    char buffer[1024];
    RequireEqual(snprintf_nid_postfix(buffer, sizeof(buffer), "[%s]", static_cast<const char*>(nullptr)), 8, "count");
    RequireEqual(Text(buffer), "[(null)]"sv, "text");
}};

const Case capacityOne{"Snprintf_CapacityOne_WritesOnlyTerminator", [] {
    char buffer[1024];
    buffer[0] = 'x';
    RequireEqual(snprintf_nid_postfix(buffer, 1, "%d", 123), 3, "count");
    RequireEqual(buffer[0], '\0', "terminator");
}};

const Case shortModifiers{"Sprintf_CharAndShortModifiers_TruncateValues", [] {
    char buffer[1024];
    RequireEqual(sprintf_nid_postfix(buffer, "%hhd %hhu %hd %hu %%", 255, 257, 65535, 65537), 11, "count");
    RequireEqual(Text(buffer), "-1 1 -1 1 %"sv, "text");
}};

const Case sprintfStackArguments{"Sprintf_SixIntegers_FormatsStackArguments", [] {
    char buffer[1024];
    RequireEqual(sprintf_nid_postfix(buffer, "%d %d %d %d %d %d", 1, 2, 3, 4, 5, 6), 11, "count");
    RequireEqual(Text(buffer), "1 2 3 4 5 6"sv, "text");
}};

const Case sscanfStackArguments{"Sscanf_EightFields_StoresStackArguments", [] {
    int first = 0, second = 0, third = 0, fourth = 0, fifth = 0, sixth = 0;
    char word[16] = {};
    double real = 0.0;
    RequireEqual(sscanf_nid_postfix("1 2 3 4 5 6 seven 8.5", "%d %d %d %d %d %d %15s %lf",
        &first, &second, &third, &fourth, &fifth, &sixth, word, &real), 8, "converted fields");
    RequireEqual(first, 1, "first");
    RequireEqual(second, 2, "second");
    RequireEqual(third, 3, "third");
    RequireEqual(fourth, 4, "fourth");
    RequireEqual(fifth, 5, "fifth");
    RequireEqual(sixth, 6, "sixth");
    RequireEqual(Text(word), "seven"sv, "word");
    RequireEqual(real, 8.5, "real");
}};

const Case countConversions{"Snprintf_CountConversions_StoreFullOutputPositions", [] {
    char buffer[1024];
    long long count = -1;
    int smallCount = -1;
    RequireEqual(snprintf_nid_postfix(buffer, 3, "abcd%lnEF%n", &count, &smallCount), 6, "result");
    RequireEqual(count, 4LL, "%ln");
    RequireEqual(smallCount, 6, "%n");
    RequireEqual(Text(buffer), "ab"sv, "text");
}};

const Case zeroCharacter{"Snprintf_ZeroCharacter_WritesEmbeddedNul", [] {
    char buffer[1024];
    RequireEqual(snprintf_nid_postfix(buffer, sizeof(buffer), "a%cb", 0), 3, "count");
    RequireEqual(std::string_view(buffer, 4), "a\0b\0"sv, "bytes");
}};

const Case printfCount{"Printf_IntegerAndDouble_ReturnsPrintedLength", [] {
    RequireEqual(printf_nid_postfix("printf: %d %.1f\n", 7, 2.5), 14, "count");
}};

const Case libcPrintfString{"LibcPrintf_String_ReturnsPrintedLength", [] {
    RequireEqual(libc_printf_nid_postfix("libc_printf: %s\n", "OK"), 16, "count");
}};

const Case libcPrintfStackArguments{"LibcPrintf_SevenIntegers_ReturnsPrintedLength", [] {
    RequireEqual(libc_printf_nid_postfix("%d %d %d %d %d %d %d\n", 1, 22, 333, 4444, 55555, 666666, 7777777), 35, "count");
}};

const Case vprintfCount{"Vprintf_Integer_ReturnsPrintedLength", [] {
    RequireEqual(PrintList("vprintf: %d\n", 42), 12, "count");
}};

} // namespace
