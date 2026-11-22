#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t None = 0;
constexpr std::int32_t Lower = 1;
constexpr std::int32_t Upper = 2;
constexpr std::int32_t Title = 3;
constexpr std::int32_t Ignorable = 4;
constexpr std::int32_t RootLocale = 1;
constexpr std::int32_t TurkishLocale = 2;
constexpr std::int32_t ArmenianLocale = 6;
constexpr std::uint32_t FoldDefault = 0;
constexpr std::uint32_t FoldExcludeSpecialI = 1;
constexpr std::int32_t Sentinel = -1;

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

struct Context {
    std::u16string text;
    std::int32_t position;
    std::int32_t index;
    std::int8_t direction;
    std::int32_t calls;
};

std::int32_t APS5_VABI Iterate(void* opaque, std::int8_t direction) {
    auto& context = *static_cast<Context*>(opaque);
    ++context.calls;
    if (direction < 0) {
        context.index = context.position;
        context.direction = -1;
    } else if (direction > 0) {
        context.index = context.position + 1;
        context.direction = 1;
    }
    if (context.direction < 0 && context.index > 0) return context.text[--context.index];
    if (context.direction > 0 && context.index < static_cast<std::int32_t>(context.text.size())) return context.text[context.index++];
    return Sentinel;
}

using FullMapping = std::int32_t (APS5_VABI*)(std::int32_t, Icu::CaseContextIterator, void*, const char16_t**, std::int32_t);

void ExpectFull(FullMapping map, std::int32_t character, std::int32_t caseLocale, const std::u16string& expected, const char* what) {
    const char16_t* string = u"unset";
    const auto result = map(character, nullptr, nullptr, &string, caseLocale);
    ExpectInt(result, static_cast<std::int64_t>(expected.size()), what);
    Expect(string && std::u16string(string, result) == expected, what);
}

void ExpectFullCodePoint(FullMapping map, std::int32_t character, std::int32_t caseLocale, std::int32_t expected, const char* what) {
    const char16_t* string = u"unset";
    ExpectInt(map(character, nullptr, nullptr, &string, caseLocale), expected, what);
    Expect(!string, what);
}

void TestProperties() {
    ExpectInt(ucase_getType_nid_postfix(u'a'), Lower, "ucase_getType(a)");
    ExpectInt(ucase_getType_nid_postfix(u'A'), Upper, "ucase_getType(A)");
    ExpectInt(ucase_getType_nid_postfix(0x01C5), Title, "ucase_getType(U+01C5)");
    ExpectInt(ucase_getType_nid_postfix(u'1'), None, "ucase_getType(1)");
    ExpectInt(ucase_getType_nid_postfix(0x0301), None, "ucase_getType(U+0301)");
    ExpectInt(ucase_getTypeOrIgnorable_nid_postfix(0x0301), Ignorable, "ucase_getTypeOrIgnorable(U+0301)");
    ExpectInt(ucase_getTypeOrIgnorable_nid_postfix(u'\''), Ignorable, "ucase_getTypeOrIgnorable(')");
    ExpectInt(ucase_getTypeOrIgnorable_nid_postfix(u'a'), Lower, "ucase_getTypeOrIgnorable(a)");
    ExpectInt(ucase_getTypeOrIgnorable_nid_postfix(u'1'), None, "ucase_getTypeOrIgnorable(1)");
    Expect(ucase_isSoftDotted_nid_postfix(u'i') && ucase_isSoftDotted_nid_postfix(u'j'), "ucase_isSoftDotted(i, j)");
    Expect(!ucase_isSoftDotted_nid_postfix(u'a') && !ucase_isSoftDotted_nid_postfix(0x0131), "ucase_isSoftDotted(a, U+0131)");
    Expect(ucase_isCaseSensitive_nid_postfix(u'a') && ucase_isCaseSensitive_nid_postfix(0x00DF), "ucase_isCaseSensitive(a, U+00DF)");
    Expect(!ucase_isCaseSensitive_nid_postfix(u'1'), "ucase_isCaseSensitive(1)");
    Expect(ucase_isCaseSensitive_nid_postfix(0x0307) && ucase_getType_nid_postfix(0x0307) == None, "ucase_isCaseSensitive(U+0307)");
    ExpectInt(ucase_getType_nid_postfix(-1), None, "ucase_getType(-1)");
    ExpectInt(ucase_getType_nid_postfix(0x110000), None, "ucase_getType(U+110000)");
}

void TestSimpleMapping() {
    ExpectInt(ucase_tolower_nid_postfix(u'A'), u'a', "ucase_tolower(A)");
    ExpectInt(ucase_tolower_nid_postfix(0x0130), u'i', "ucase_tolower(U+0130)");
    ExpectInt(ucase_tolower_nid_postfix(0x10400), 0x10428, "ucase_tolower(U+10400)");
    ExpectInt(ucase_toupper_nid_postfix(u'a'), u'A', "ucase_toupper(a)");
    ExpectInt(ucase_toupper_nid_postfix(0x00DF), 0x00DF, "ucase_toupper(U+00DF)");
    ExpectInt(ucase_toupper_nid_postfix(0x01C6), 0x01C4, "ucase_toupper(U+01C6)");
    ExpectInt(ucase_totitle_nid_postfix(0x01C6), 0x01C5, "ucase_totitle(U+01C6)");
    ExpectInt(ucase_totitle_nid_postfix(u'1'), u'1', "ucase_totitle(1)");
    ExpectInt(ucase_fold_nid_postfix(u'I', FoldDefault), u'i', "ucase_fold(I)");
    ExpectInt(ucase_fold_nid_postfix(u'I', FoldExcludeSpecialI), 0x0131, "ucase_fold(I, Turkic)");
    ExpectInt(ucase_fold_nid_postfix(0x0130, FoldDefault), 0x0130, "ucase_fold(U+0130)");
    ExpectInt(ucase_fold_nid_postfix(0x0130, FoldExcludeSpecialI), u'i', "ucase_fold(U+0130, Turkic)");
    ExpectInt(ucase_fold_nid_postfix(0x1E9E, FoldDefault), 0x00DF, "ucase_fold(U+1E9E)");
}

void TestFullMapping() {
    ExpectFullCodePoint(ucase_toFullLower_nid_postfix, u'A', RootLocale, u'a', "ucase_toFullLower(A)");
    ExpectFullCodePoint(ucase_toFullLower_nid_postfix, u'1', RootLocale, ~std::int32_t{u'1'}, "ucase_toFullLower(1)");
    ExpectFull(ucase_toFullLower_nid_postfix, 0x0130, RootLocale, u"i̇", "ucase_toFullLower(U+0130)");
    ExpectFullCodePoint(ucase_toFullLower_nid_postfix, 0x0130, TurkishLocale, u'i', "ucase_toFullLower(U+0130, tr)");
    ExpectFullCodePoint(ucase_toFullLower_nid_postfix, u'I', TurkishLocale, 0x0131, "ucase_toFullLower(I, tr)");
    ExpectFull(ucase_toFullUpper_nid_postfix, 0x00DF, RootLocale, u"SS", "ucase_toFullUpper(U+00DF)");
    ExpectFullCodePoint(ucase_toFullUpper_nid_postfix, u'i', TurkishLocale, 0x0130, "ucase_toFullUpper(i, tr)");
    ExpectFull(ucase_toFullUpper_nid_postfix, 0x0587, RootLocale, u"ԵՒ", "ucase_toFullUpper(U+0587)");
    ExpectFull(ucase_toFullUpper_nid_postfix, 0x0587, ArmenianLocale, u"ԵՎ", "ucase_toFullUpper(U+0587, hy)");
    ExpectFull(ucase_toFullTitle_nid_postfix, 0x00DF, RootLocale, u"Ss", "ucase_toFullTitle(U+00DF)");
    ExpectFull(ucase_toFullTitle_nid_postfix, 0x0587, ArmenianLocale, u"Եվ", "ucase_toFullTitle(U+0587, hy)");
    ExpectFullCodePoint(ucase_toFullTitle_nid_postfix, 0x01C6, RootLocale, 0x01C5, "ucase_toFullTitle(U+01C6)");

    const char16_t* string = u"unset";
    ExpectInt(ucase_toFullFolding_nid_postfix(0x00DF, &string, FoldDefault), 2, "ucase_toFullFolding(U+00DF)");
    Expect(string && std::u16string(string, 2) == u"ss", "ucase_toFullFolding(U+00DF) string");
    ExpectInt(ucase_toFullFolding_nid_postfix(0x0130, &string, FoldDefault), 2, "ucase_toFullFolding(U+0130)");
    Expect(string && std::u16string(string, 2) == u"i̇", "ucase_toFullFolding(U+0130) string");
    ExpectInt(ucase_toFullFolding_nid_postfix(0x0130, &string, FoldExcludeSpecialI), u'i', "ucase_toFullFolding(U+0130, Turkic)");
    Expect(!string, "ucase_toFullFolding(U+0130, Turkic) string");
    ExpectInt(ucase_toFullFolding_nid_postfix(u'a', &string, FoldDefault), ~std::int32_t{u'a'}, "ucase_toFullFolding(a)");

    ExpectThrow<std::invalid_argument>([] { ucase_toFullLower_nid_postfix(u'A', nullptr, nullptr, nullptr, RootLocale); }, "ucase_toFullLower null string");
    ExpectThrow<std::invalid_argument>([] { ucase_toFullUpper_nid_postfix(u'a', nullptr, nullptr, nullptr, RootLocale); }, "ucase_toFullUpper null string");
    ExpectThrow<std::invalid_argument>([] { ucase_toFullTitle_nid_postfix(u'a', nullptr, nullptr, nullptr, RootLocale); }, "ucase_toFullTitle null string");
    ExpectThrow<std::invalid_argument>([] { ucase_toFullFolding_nid_postfix(u'A', nullptr, FoldDefault); }, "ucase_toFullFolding null string");
}

std::int32_t LowerSigma(const std::u16string& text, std::int32_t position, std::int32_t& calls) {
    Context context{text, position, 0, 0, 0};
    const char16_t* string = u"unset";
    const auto result = ucase_toFullLower_nid_postfix(0x03A3, Iterate, &context, &string, RootLocale);
    Expect(!string, "ucase_toFullLower(sigma) string");
    calls = context.calls;
    return result;
}

void TestContext() {
    std::int32_t calls = 0;
    ExpectInt(LowerSigma(u"ΑΣ", 1, calls), 0x03C2, "ucase_toFullLower(final sigma)");
    Expect(calls > 0, "ucase_toFullLower(final sigma) calls the iterator");
    ExpectInt(LowerSigma(u"ΑΣΑ", 1, calls), 0x03C3, "ucase_toFullLower(medial sigma)");
    ExpectInt(LowerSigma(u"Σ", 0, calls), 0x03C3, "ucase_toFullLower(lone sigma)");
    ExpectInt(LowerSigma(u"Α'Σ", 2, calls), 0x03C2, "ucase_toFullLower(sigma after ignorable)");
    const char16_t* string = u"unset";
    ExpectInt(ucase_toFullLower_nid_postfix(0x03A3, nullptr, nullptr, &string, RootLocale), 0x03C3, "ucase_toFullLower(sigma, no iterator)");
}

}

int main() {
    TestProperties();
    TestSimpleMapping();
    TestFullMapping();
    TestContext();
    std::puts("ICU case property tests passed");
}
