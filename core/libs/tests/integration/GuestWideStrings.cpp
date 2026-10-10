#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <climits>
#include <cstddef>
#include <initializer_list>
#include <string>

extern "C" {
const char16_t* APS5_VABI wmemchr_nid_postfix(const char16_t* s, char16_t c, std::size_t n);
int APS5_VABI wmemcmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::size_t n);
char16_t* APS5_VABI wmemcpy_nid_postfix(char16_t* dest, const char16_t* src, std::size_t n);
char16_t* APS5_VABI wmemmove_nid_postfix(char16_t* dest, const char16_t* src, std::size_t n);
std::size_t APS5_VABI wcslen_nid_postfix(const char16_t* s);
int APS5_VABI wcscmp_nid_postfix(const char16_t* s1, const char16_t* s2);
int APS5_VABI wcsncmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::size_t n);
char16_t* APS5_VABI wcscpy_nid_postfix(char16_t* dest, const char16_t* src);
char16_t* APS5_VABI wcsncpy_nid_postfix(char16_t* dest, const char16_t* src, std::size_t n);
const char16_t* APS5_VABI wcschr_nid_postfix(const char16_t* s, char16_t c);
const char16_t* APS5_VABI wcsrchr_nid_postfix(const char16_t* s, char16_t c);
const char16_t* APS5_VABI wcsstr_nid_postfix(const char16_t* haystack, const char16_t* needle);
const char16_t* APS5_VABI wcspbrk_nid_postfix(const char16_t* s, const char16_t* accept);
std::size_t APS5_VABI wcsspn_nid_postfix(const char16_t* s, const char16_t* accept);
char16_t* APS5_VABI wmemset_nid_postfix(char16_t* s, char16_t c, std::size_t n);
double APS5_VABI wcstod_nid_postfix(const char16_t* str, char16_t** endptr);
float APS5_VABI wcstof_nid_postfix(const char16_t* str, char16_t** endptr);
long double APS5_VABI wcstold_nid_postfix(const char16_t* str, char16_t** endptr);
long long APS5_VABI wcstol_nid_postfix(const char16_t* str, char16_t** endptr, int base);
long long APS5_VABI wcstoll_nid_postfix(const char16_t* str, char16_t** endptr, int base);
unsigned long long APS5_VABI wcstoul_nid_postfix(const char16_t* str, char16_t** endptr, int base);
unsigned long long APS5_VABI wcstoull_nid_postfix(const char16_t* str, char16_t** endptr, int base);
int APS5_VABI wcscoll_nid_postfix(const char16_t* first, const char16_t* second);
std::size_t APS5_VABI wcsxfrm_nid_postfix(char16_t* destination, const char16_t* source, std::size_t count);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

int Sign(int value) {
    return (value > 0) - (value < 0);
}

void RequireUnits(const char16_t* actual, std::initializer_list<char16_t> expected, const std::string& message) {
    std::size_t index = 0;
    for (const char16_t unit : expected) {
        RequireEqual(static_cast<int>(actual[index]), static_cast<int>(unit), message + " at unit " + std::to_string(index));
        ++index;
    }
}

void RequireAt(const char16_t* actual, const char16_t* base, std::ptrdiff_t offset, const std::string& message) {
    Require(actual != nullptr, message + ": not null");
    RequireEqual(actual - base, offset, message + ": offset");
}

const Case wcslenCase{"Wcslen_Strings_CountUnitsBeforeTerminator", [] {
    const char16_t split[] = {u'a', u'b', 0, u'c', 0};
    RequireEqual(wcslen_nid_postfix(u""), std::size_t{0}, "empty");
    RequireEqual(wcslen_nid_postfix(u"héllo"), std::size_t{5}, "non-ascii");
    RequireEqual(wcslen_nid_postfix(split), std::size_t{2}, "embedded terminator");
}};

const Case wcscmpCase{"Wcscmp_Strings_OrderByUnsignedUnits", [] {
    RequireEqual(Sign(wcscmp_nid_postfix(u"abc", u"abc")), 0, "equal");
    RequireEqual(Sign(wcscmp_nid_postfix(u"abc", u"abd")), -1, "abc vs abd");
    RequireEqual(Sign(wcscmp_nid_postfix(u"abd", u"abc")), 1, "abd vs abc");
    RequireEqual(Sign(wcscmp_nid_postfix(u"ab", u"abc")), -1, "prefix");
    RequireEqual(Sign(wcscmp_nid_postfix(u"￿", u"a")), 1, "U+FFFF vs a");
    RequireEqual(Sign(wcscmp_nid_postfix(u"耀", u"翿")), 1, "U+8000 vs U+7FFF");
}};

const Case wcsncmpCase{"Wcsncmp_Limits_CompareOnlyLeadingUnits", [] {
    RequireEqual(Sign(wcsncmp_nid_postfix(u"abcx", u"abcy", 3)), 0, "three units");
    RequireEqual(Sign(wcsncmp_nid_postfix(u"abcx", u"abcy", 4)), -1, "four units");
    RequireEqual(Sign(wcsncmp_nid_postfix(u"ab", u"ab", 10)), 0, "limit past terminator");
    RequireEqual(Sign(wcsncmp_nid_postfix(u"a", u"b", 0)), 0, "zero limit");
}};

const Case wcscpyCase{"Wcscpy_Source_CopiesThroughTerminatorOnly", [] {
    char16_t buffer[8];
    wmemset_nid_postfix(buffer, 0xaaaa, 8);
    Require(wcscpy_nid_postfix(buffer, u"wide") == buffer, "returns the destination");
    RequireUnits(buffer, {u'w', u'i', u'd', u'e', 0, 0xaaaa}, "buffer");
}};

const Case wcsncpyPadCase{"Wcsncpy_ShortSource_PadsWithTerminators", [] {
    char16_t buffer[8];
    wmemset_nid_postfix(buffer, 0xaaaa, 8);
    Require(wcsncpy_nid_postfix(buffer, u"ab", 5) == buffer, "returns the destination");
    RequireUnits(buffer, {u'a', u'b', 0, 0, 0, 0xaaaa}, "buffer");
}};

const Case wcsncpyTruncateCase{"Wcsncpy_LongSource_TruncatesWithoutTerminator", [] {
    char16_t buffer[8];
    wmemset_nid_postfix(buffer, 0xaaaa, 8);
    wcsncpy_nid_postfix(buffer, u"abcdef", 3);
    RequireUnits(buffer, {u'a', u'b', u'c', 0xaaaa}, "buffer");
}};

const Case wcschrCase{"Wcschr_Characters_FindFirstOccurrence", [] {
    const char16_t* text = u"a世b世c";
    RequireAt(wcschr_nid_postfix(text, u'世'), text, 1, "U+4E16");
    Require(wcschr_nid_postfix(text, u'z') == nullptr, "missing character");
    RequireAt(wcschr_nid_postfix(text, 0), text, 5, "terminator");
}};

const Case wcsrchrCase{"Wcsrchr_Characters_FindLastOccurrence", [] {
    const char16_t* text = u"a世b世c";
    RequireAt(wcsrchr_nid_postfix(text, u'世'), text, 3, "U+4E16");
    Require(wcsrchr_nid_postfix(text, u'z') == nullptr, "missing character");
    RequireAt(wcsrchr_nid_postfix(text, 0), text, 5, "terminator");
}};

const Case wcsstrCase{"Wcsstr_Needles_FindFirstMatch", [] {
    const char16_t* haystack = u"one two two";
    RequireAt(wcsstr_nid_postfix(haystack, u"two"), haystack, 4, "two");
    RequireAt(wcsstr_nid_postfix(haystack, u""), haystack, 0, "empty needle");
    Require(wcsstr_nid_postfix(haystack, u"three") == nullptr, "missing needle");
    Require(wcsstr_nid_postfix(u"tw", u"two") == nullptr, "needle longer than haystack");
}};

const Case wcspbrkCase{"Wcspbrk_AcceptSets_FindFirstMember", [] {
    const char16_t* haystack = u"one two two";
    RequireAt(wcspbrk_nid_postfix(haystack, u"wt"), haystack, 4, "wt");
    Require(wcspbrk_nid_postfix(haystack, u"xyz") == nullptr, "no member");
}};

const Case wcsspnCase{"Wcsspn_AcceptSets_CountLeadingMembers", [] {
    RequireEqual(wcsspn_nid_postfix(u"aabbc", u"ab"), std::size_t{4}, "ab");
    RequireEqual(wcsspn_nid_postfix(u"abc", u""), std::size_t{0}, "empty set");
}};

const Case wmemchrCase{"Wmemchr_Units_SearchOnlyGivenLength", [] {
    const char16_t units[] = {u'x', 0, u'￿', u'y'};
    RequireAt(wmemchr_nid_postfix(units, u'￿', 4), units, 2, "U+FFFF past an embedded terminator");
    Require(wmemchr_nid_postfix(units, u'y', 3) == nullptr, "unit beyond the length");
}};

const Case wmemcmpCase{"Wmemcmp_Units_CompareUnsignedWithinLength", [] {
    const char16_t units[] = {u'x', 0, u'￿', u'y'};
    const char16_t lower[] = {u'x', 0, u'\u0001', u'y'};
    RequireEqual(Sign(wmemcmp_nid_postfix(units, units, 4)), 0, "same");
    RequireEqual(Sign(wmemcmp_nid_postfix(units, lower, 4)), 1, "U+FFFF vs U+0001");
    RequireEqual(Sign(wmemcmp_nid_postfix(lower, units, 4)), -1, "U+0001 vs U+FFFF");
    RequireEqual(Sign(wmemcmp_nid_postfix(units, lower, 2)), 0, "difference beyond the length");
}};

const Case wmemcpyCase{"Wmemcpy_Units_CopiesEmbeddedTerminators", [] {
    const char16_t units[] = {u'x', 0, u'￿', u'y'};
    char16_t copy[4] = {};
    Require(wmemcpy_nid_postfix(copy, units, 4) == copy, "returns the destination");
    RequireUnits(copy, {u'x', 0, u'￿', u'y'}, "copy");
}};

const Case wmemmoveCase{"Wmemmove_OverlappingRanges_CopiesAsIfBuffered", [] {
    char16_t overlap[] = {u'1', u'2', u'3', u'4', u'5'};
    Require(wmemmove_nid_postfix(overlap + 1, overlap, 4) == overlap + 1, "returns the destination");
    RequireUnits(overlap, {u'1', u'1', u'2', u'3', u'4'}, "buffer");
}};

const Case wmemsetCase{"Wmemset_Count_FillsOnlyGivenUnits", [] {
    char16_t copy[4] = {u'x', 0, u'￿', u'y'};
    Require(wmemset_nid_postfix(copy, u'世', 3) == copy, "returns the destination");
    RequireUnits(copy, {u'世', u'世', u'世', u'y'}, "buffer");
}};

const Case wcstolNegativeCase{"Wcstol_LeadingSpacesAndSign_ParsesNegativeDecimal", [] {
    char16_t* end = nullptr;
    const char16_t* negative = u"  -42xyz";
    RequireEqual(wcstol_nid_postfix(negative, &end, 10), -42LL, "value");
    RequireEqual(end - negative, std::ptrdiff_t{5}, "end offset");
}};

const Case wcstollHexCase{"Wcstoll_HexPrefix_ParsesInBase16AndAutoBase", [] {
    const char16_t* hex = u"0x1F!";
    for (const int base : {16, 0}) {
        char16_t* end = nullptr;
        const std::string label = "base " + std::to_string(base);
        RequireEqual(wcstoll_nid_postfix(hex, &end, base), 31LL, label + " value");
        RequireEqual(end - hex, std::ptrdiff_t{4}, label + " end offset");
    }
}};

const Case wcstoulWideSpaceCase{"Wcstoul_IdeographicSpace_IsNotSkipped", [] {
    char16_t* end = nullptr;
    const char16_t* wideSpace = u"　12";
    RequireEqual(wcstoul_nid_postfix(wideSpace, &end, 10), 0ULL, "value");
    RequireEqual(end - wideSpace, std::ptrdiff_t{0}, "end offset");
}};

const Case wcstollWideDigitCase{"Wcstoll_ArabicIndicDigit_StopsParsing", [] {
    char16_t* end = nullptr;
    const char16_t* wideDigit = u"12١";
    RequireEqual(wcstoll_nid_postfix(wideDigit, &end, 10), 12LL, "value");
    RequireEqual(end - wideDigit, std::ptrdiff_t{2}, "end offset");
}};

const Case wcstoullMaxCase{"Wcstoull_UnsignedMaximum_ParsesWithoutOverflow", [] {
    RequireEqual(wcstoull_nid_postfix(u"18446744073709551615", nullptr, 10), ULLONG_MAX, "value");
}};

const Case wcstolLettersCase{"Wcstol_NoDigits_ReturnsZeroAndStart", [] {
    char16_t* end = nullptr;
    const char16_t* letters = u"abc";
    RequireEqual(wcstol_nid_postfix(letters, &end, 10), 0LL, "value");
    RequireEqual(end - letters, std::ptrdiff_t{0}, "end offset");
}};

const Case binaryPrefixCase{"WideIntegerParsers_BinaryPrefix_StopAfterZero", [] {
    const char16_t* binary = u" -0b101";
    for (const int base : {0, 2}) {
        char16_t* end = nullptr;
        const std::string label = " in base " + std::to_string(base);
        RequireEqual(wcstol_nid_postfix(binary, &end, base), 0LL, "wcstol" + label);
        RequireEqual(end - binary, std::ptrdiff_t{3}, "wcstol end" + label);
        RequireEqual(wcstoll_nid_postfix(binary, &end, base), 0LL, "wcstoll" + label);
        RequireEqual(end - binary, std::ptrdiff_t{3}, "wcstoll end" + label);
        RequireEqual(wcstoul_nid_postfix(binary, &end, base), 0ULL, "wcstoul" + label);
        RequireEqual(end - binary, std::ptrdiff_t{3}, "wcstoul end" + label);
        RequireEqual(wcstoull_nid_postfix(binary, &end, base), 0ULL, "wcstoull" + label);
        RequireEqual(end - binary, std::ptrdiff_t{3}, "wcstoull end" + label);
    }
}};

const Case binaryDigitsInHexCase{"Wcstoll_BinaryLookingDigitsInBase16_ParseAsHex", [] {
    char16_t* end = nullptr;
    const char16_t* binary = u" -0b101";
    RequireEqual(wcstoll_nid_postfix(binary + 2, &end, 16), 0xb101LL, "value");
    RequireEqual(end - binary, std::ptrdiff_t{7}, "end offset");
}};

const Case wcstodScientificCase{"Wcstod_Exponent_ParsesValueAndStopsAtTail", [] {
    char16_t* end = nullptr;
    const char16_t* scientific = u"3.5e2!";
    RequireEqual(wcstod_nid_postfix(scientific, &end), 350.0, "value");
    RequireEqual(end - scientific, std::ptrdiff_t{5}, "end offset");
}};

const Case wcstodLettersCase{"Wcstod_NoDigits_ReturnsZeroAndStart", [] {
    char16_t* end = nullptr;
    const char16_t* letters = u"abc";
    RequireEqual(wcstod_nid_postfix(letters, &end), 0.0, "value");
    RequireEqual(end - letters, std::ptrdiff_t{0}, "end offset");
}};

const Case wcstofCase{"Wcstof_Fraction_ParsesFloat", [] {
    RequireEqual(wcstof_nid_postfix(u"0.25", nullptr), 0.25f, "value");
}};

const Case wcstoldCase{"Wcstold_NegativeFraction_ParsesAndStopsAtNonAscii", [] {
    char16_t* end = nullptr;
    const char16_t* half = u"-1.5é";
    Require(wcstold_nid_postfix(half, &end) == -1.5L, "value is -1.5");
    RequireEqual(end - half, std::ptrdiff_t{4}, "end offset");
}};

const Case wcscollCase{"Wcscoll_Strings_OrderLikeUnits", [] {
    RequireEqual(Sign(wcscoll_nid_postfix(u"a", u"b")), -1, "a vs b");
    RequireEqual(Sign(wcscoll_nid_postfix(u"￿", u"a")), 1, "U+FFFF vs a");
    RequireEqual(Sign(wcscoll_nid_postfix(u"same", u"same")), 0, "equal");
}};

const Case wcsxfrmFitsCase{"Wcsxfrm_FittingSource_CopiesThroughTerminatorOnly", [] {
    char16_t transformed[8];
    wmemset_nid_postfix(transformed, 0xaaaa, 8);
    RequireEqual(wcsxfrm_nid_postfix(transformed, u"wide", 8), std::size_t{4}, "length");
    RequireUnits(transformed, {u'w', u'i', u'd', u'e', 0, 0xaaaa}, "buffer");
}};

const Case wcsxfrmTooLongCase{"Wcsxfrm_TooLongSource_ReturnsRequiredLength", [] {
    char16_t transformed[8];
    wmemset_nid_postfix(transformed, 0xaaaa, 8);
    RequireEqual(wcsxfrm_nid_postfix(transformed, u"much too long", 4), std::size_t{13}, "length");
}};

const Case wcsxfrmNullCase{"Wcsxfrm_NullDestinationAndZeroCount_ReturnsRequiredLength", [] {
    RequireEqual(wcsxfrm_nid_postfix(nullptr, u"abc", 0), std::size_t{3}, "length");
}};

} // namespace
