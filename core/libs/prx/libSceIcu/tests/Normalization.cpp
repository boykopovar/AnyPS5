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
constexpr std::int32_t Compose = 0;
constexpr std::int32_t Decompose = 1;
constexpr std::int32_t No = 0;
constexpr std::int32_t Yes = 1;
constexpr std::int32_t Maybe = 2;
constexpr std::uint32_t ExcludeSpecialI = 1;
constexpr std::uint32_t CodePointOrder = 0x8000;
constexpr std::uint32_t IgnoreCase = 0x10000;
constexpr std::uint32_t Unicode32 = 0x20u << 20;

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

const Icu::Normalizer2* Nfc() {
    std::int32_t error = Success;
    const auto* normalizer = unorm2_getNFCInstance_nid_postfix(&error);
    ExpectInt(error, Success, "unorm2_getNFCInstance");
    return normalizer;
}

const Icu::Normalizer2* Nfd() {
    std::int32_t error = Success;
    const auto* normalizer = unorm2_getNFDInstance_nid_postfix(&error);
    ExpectInt(error, Success, "unorm2_getNFDInstance");
    return normalizer;
}

void ExpectNormalized(const Icu::Normalizer2* normalizer, const std::u16string& source, const std::u16string& expected,
                      const char* what) {
    char16_t buffer[16] = {};
    std::int32_t error = Success;
    const auto length = unorm2_normalize_nid_postfix(normalizer, source.c_str(), -1, buffer, 16, &error);
    ExpectInt(error, Success, what);
    ExpectInt(length, static_cast<std::int64_t>(expected.size()), what);
    Expect(std::u16string(buffer, length) == expected && buffer[length] == 0, what);
}

void TestInstances() {
    const auto* nfc = Nfc();
    const auto* nfd = Nfd();
    Expect(nfc != nullptr && nfd != nullptr && nfc != nfd, "NFC and NFD instances");
    std::int32_t error = Success;
    Expect(unorm2_getInstance_nid_postfix(nullptr, "nfc", Compose, &error) == nfc, "unorm2_getInstance(nfc, compose)");
    Expect(unorm2_getInstance_nid_postfix(nullptr, "nfc", Decompose, &error) == nfd, "unorm2_getInstance(nfc, decompose)");
    ExpectInt(error, Success, "unorm2_getInstance status");
    Expect(unorm2_getInstance_nid_postfix(nullptr, "", Compose, &error) == nullptr, "unorm2_getInstance(empty name)");
    ExpectInt(error, IllegalArgument, "unorm2_getInstance(empty name) status");
}

void TestNormalize() {
    const auto* nfc = Nfc();
    const auto* nfd = Nfd();
    ExpectNormalized(nfc, u"é", u"é", "NFC(e + acute)");
    ExpectNormalized(nfd, u"é", u"é", "NFD(U+00E9)");
    ExpectNormalized(nfd, u"Ḉ", u"Ḉ", "NFD(U+1E08)");
    ExpectNormalized(nfc, u"ậ", u"ậ", "NFC(a + dot below + circumflex)");
    ExpectNormalized(nfc, u"가", u"가", "NFC(Hangul syllable)");
    ExpectNormalized(nfd, u"가", u"가", "NFD(Hangul syllable)");

    std::int32_t error = Success;
    ExpectInt(unorm2_normalize_nid_postfix(nfd, u"é", -1, nullptr, 0, &error), 2, "unorm2_normalize preflight");
    ExpectInt(error, BufferOverflow, "unorm2_normalize preflight status");

    error = Success;
    char16_t same[4] = u"abc";
    ExpectInt(unorm2_normalize_nid_postfix(nfc, same, -1, same, 4, &error), 0, "unorm2_normalize in place");
    ExpectInt(error, IllegalArgument, "unorm2_normalize in place status");
}

void TestCompatibilityInstances() {
    std::int32_t error = Success;
    const auto* nfkc = unorm2_getNFKCInstance(&error);
    const auto* nfkd = unorm2_getNFKDInstance(&error);
    const auto* nfkcCf = unorm2_getNFKCCasefoldInstance_nid_postfix(&error);
    Expect(nfkc && nfkd && nfkcCf && error == Success, "compatibility normalizers");
    Expect(unorm2_getInstance_nid_postfix(nullptr, "nfkc", Compose, &error) == nfkc, "NFKC instance lookup");
    Expect(unorm2_getInstance_nid_postfix(nullptr, "nfkc", Decompose, &error) == nfkd, "NFKD instance lookup");
    Expect(unorm2_getInstance_nid_postfix(nullptr, "nfkc_cf", Compose, &error) == nfkcCf, "NFKC casefold instance lookup");
    char16_t output[16]{};
    ExpectInt(unorm2_normalize_nid_postfix(nfkc, u"\uff21\ufb03\u00e9", -1, output, 16, &error), 5, "NFKC length");
    Expect(std::u16string(output) == u"Affi\u00e9", "NFKC text");
    ExpectInt(unorm2_normalize_nid_postfix(nfkd, u"\uff21\ufb03\u00e9", -1, output, 16, &error), 6, "NFKD length");
    Expect(std::u16string(output) == u"Affie\u0301", "NFKD text");
    ExpectInt(unorm2_normalize_nid_postfix(nfkcCf, u"\uff21\u00ad\u00df", -1, output, 16, &error), 3, "NFKC casefold length");
    Expect(std::u16string(output) == u"ass", "NFKC casefold text");
    ExpectInt(u_getFC_NFKC_Closure_nid_postfix(0x037a, output, 16, &error), 2, "casefold closure length");
    Expect(std::u16string(output) == u" \u03b9" && error == Success, "casefold closure");
}

void TestQueries() {
    const auto* nfc = Nfc();
    const auto* nfd = Nfd();
    std::int32_t error = Success;
    ExpectInt(unorm2_isNormalized_nid_postfix(nfc, u"é", -1, &error), 1, "unorm2_isNormalized(NFC, U+00E9)");
    ExpectInt(unorm2_isNormalized_nid_postfix(nfc, u"é", -1, &error), 0, "unorm2_isNormalized(NFC, e + acute)");
    ExpectInt(unorm2_quickCheck_nid_postfix(nfc, u"é", -1, &error), Yes, "unorm2_quickCheck(NFC, U+00E9)");
    ExpectInt(unorm2_quickCheck_nid_postfix(nfc, u"é", -1, &error), Maybe, "unorm2_quickCheck(NFC, e + acute)");
    ExpectInt(unorm2_quickCheck_nid_postfix(nfd, u"é", -1, &error), No, "unorm2_quickCheck(NFD, U+00E9)");
    ExpectInt(unorm2_spanQuickCheckYes_nid_postfix(nfc, u"abć", -1, &error), 2, "unorm2_spanQuickCheckYes");
    ExpectInt(error, Success, "query status");

    ExpectInt(unorm2_composePair_nid_postfix(nfc, u'e', 0x0301), 0x00E9, "unorm2_composePair(e, acute)");
    ExpectInt(unorm2_composePair_nid_postfix(nfc, u'a', u'b'), -1, "unorm2_composePair(a, b)");
    ExpectInt(unorm2_getCombiningClass_nid_postfix(nfc, 0x0301), 230, "unorm2_getCombiningClass(acute)");
    ExpectInt(unorm2_getCombiningClass_nid_postfix(nfc, 0x0327), 202, "unorm2_getCombiningClass(cedilla)");
    ExpectInt(unorm2_getCombiningClass_nid_postfix(nfc, u'a'), 0, "unorm2_getCombiningClass(a)");
    ExpectInt(unorm2_hasBoundaryBefore_nid_postfix(nfc, u'a'), 1, "unorm2_hasBoundaryBefore(a)");
    ExpectInt(unorm2_hasBoundaryBefore_nid_postfix(nfc, 0x0301), 0, "unorm2_hasBoundaryBefore(acute)");
    ExpectInt(unorm2_hasBoundaryAfter_nid_postfix(nfc, u'a'), 0, "unorm2_hasBoundaryAfter(a)");
    ExpectInt(unorm2_hasBoundaryAfter_nid_postfix(nfc, u'!'), 1, "unorm2_hasBoundaryAfter(!)");
    ExpectInt(unorm2_isInert_nid_postfix(nfc, u'a'), 0, "unorm2_isInert(a)");
    ExpectInt(unorm2_isInert_nid_postfix(nfc, u'!'), 1, "unorm2_isInert(!)");
}

void TestDecompositions() {
    const auto* nfc = Nfc();
    char16_t buffer[8] = {};
    std::int32_t error = Success;
    ExpectInt(unorm2_getDecomposition_nid_postfix(nfc, 0x1E08, buffer, 8, &error), 3, "unorm2_getDecomposition(U+1E08)");
    Expect(std::u16string(buffer) == u"Ḉ", "unorm2_getDecomposition(U+1E08) text");
    ExpectInt(unorm2_getRawDecomposition_nid_postfix(nfc, 0x1E08, buffer, 8, &error), 2, "unorm2_getRawDecomposition(U+1E08)");
    Expect(std::u16string(buffer) == u"Ḉ", "unorm2_getRawDecomposition(U+1E08) text");
    ExpectInt(unorm2_getDecomposition_nid_postfix(nfc, u'a', buffer, 8, &error), -1, "unorm2_getDecomposition(a)");
    ExpectInt(unorm2_getRawDecomposition_nid_postfix(nfc, u'a', buffer, 8, &error), -1, "unorm2_getRawDecomposition(a)");
    ExpectInt(error, Success, "decomposition status");
}

void TestAppend() {
    const auto* nfc = Nfc();
    std::int32_t error = Success;
    char16_t appended[8] = u"a";
    ExpectInt(unorm2_append_nid_postfix(nfc, appended, -1, 8, u"xé", -1, &error), 4, "unorm2_append");
    Expect(std::u16string(appended) == u"axé", "unorm2_append text");
    char16_t normalized[8] = u"a";
    ExpectInt(unorm2_normalizeSecondAndAppend_nid_postfix(nfc, normalized, -1, 8, u"xé", -1, &error), 3,
              "unorm2_normalizeSecondAndAppend");
    Expect(std::u16string(normalized) == u"axé", "unorm2_normalizeSecondAndAppend text");
    char16_t boundary[8] = u"e";
    ExpectInt(unorm2_append_nid_postfix(nfc, boundary, -1, 8, u"́", -1, &error), 1, "unorm2_append at a boundary");
    Expect(std::u16string(boundary) == u"é", "unorm2_append at a boundary text");
    ExpectInt(error, Success, "append status");
}

void TestArguments() {
    std::int32_t failing = IllegalArgument;
    Expect(unorm2_getNFCInstance_nid_postfix(&failing) == nullptr, "unorm2_getNFCInstance with failing status");
    ExpectInt(unorm2_normalize_nid_postfix(nullptr, u"a", -1, nullptr, 0, &failing), 0, "unorm2_normalize with failing status");
    ExpectInt(failing, IllegalArgument, "failing status unchanged");
    std::int32_t error = Success;
    ExpectInt(unorm2_normalize_nid_postfix(nullptr, u"", 0, nullptr, 0, &error), 0, "unorm2_normalize(null, empty)");
    ExpectThrow<std::invalid_argument>([] { unorm2_getNFCInstance_nid_postfix(nullptr); }, "unorm2_getNFCInstance(null status)");
    ExpectThrow<std::invalid_argument>([] {
        std::int32_t status = Success;
        unorm2_normalize_nid_postfix(nullptr, u"a", -1, nullptr, 0, &status);
    }, "unorm2_normalize(null normalizer)");
    ExpectThrow<std::invalid_argument>([] { unorm2_composePair_nid_postfix(nullptr, u'e', 0x0301); }, "unorm2_composePair(null)");
}

void TestFiltered() {
    const auto* nfc = Nfc();
    std::int32_t error = Success;
    Expect(unorm2_openFiltered_nid_postfix(nfc, nullptr, &error) == nullptr && error == IllegalArgument,
           "filtered normalizer requires a set");
    auto* set = uset_openEmpty_nid_postfix();
    Expect(set != nullptr, "allocate normalization set");
    Expect(unorm2_openFiltered_nid_postfix(nfc, set, &error) == nullptr && error == IllegalArgument,
           "filtered normalizer preserves incoming failure");
    error = Success;
    uset_add_nid_postfix(set, u'e');
    uset_add_nid_postfix(set, 0x0301);
    auto* filtered = unorm2_openFiltered_nid_postfix(nfc, set, &error);
    Expect(filtered != nullptr && error == Success, "open filtered NFC");
    char16_t result[16]{};
    ExpectInt(unorm2_normalize_nid_postfix(filtered, u"e\u0301a\u0301", -1, result, 16, &error), 3, "filtered output length");
    Expect(std::u16string(result) == u"\u00e9a\u0301", "only matching runs are normalized");
    ExpectInt(unorm2_composePair_nid_postfix(filtered, u'e', 0x0301), 0x00e9, "included pair composes");
    ExpectInt(unorm2_composePair_nid_postfix(filtered, u'a', 0x0301), -1, "excluded pair is unchanged");
    unorm2_close_nid_postfix(filtered);
    unorm2_close_nid_postfix(nullptr);
    uset_close_nid_postfix(set);
}

std::int32_t Compare(const char16_t* first, std::int32_t firstLength, const char16_t* second, std::int32_t secondLength,
                     std::uint32_t options, const char* what) {
    std::int32_t error = Success;
    const auto result = unorm_compare_nid_postfix(first, firstLength, second, secondLength, options, &error);
    ExpectInt(error, Success, what);
    return result;
}

void TestCompare() {
    ExpectInt(Compare(u"\u00e9", -1, u"e\u0301", -1, 0, "precomposed and decomposed e acute"), 0, "canonical equivalence");
    ExpectInt(Compare(u"a\u0323\u0302", -1, u"a\u0302\u0323", -1, 0, "reordered marks"), 0, "canonical reordering");
    Expect(Compare(u"abc", -1, u"abd", -1, 0, "abc and abd") < 0, "abc sorts before abd");
    Expect(Compare(u"abd", -1, u"abc", -1, 0, "abd and abc") > 0, "abd sorts after abc");
    ExpectInt(Compare(u"abcX", 3, u"abc", -1, 0, "explicit length"), 0, "explicit length bounds the first string");
    Expect(Compare(u"\u00c9", -1, u"e\u0301", -1, 0, "case-sensitive E acute") != 0, "case-sensitive comparison differs");
    ExpectInt(Compare(u"\u00c9", -1, u"e\u0301", -1, IgnoreCase, "caseless E acute"), 0, "canonical caseless match");
    ExpectInt(Compare(u"\u0130", -1, u"i\u0307", -1, IgnoreCase, "dotted capital I"), 0, "full case folding of U+0130");
    ExpectInt(Compare(u"I", -1, u"i", -1, IgnoreCase, "default I folding"), 0, "I folds to i by default");
    Expect(Compare(u"I", -1, u"\u0131", -1, IgnoreCase, "default dotless i") != 0, "I does not fold to dotless i by default");
    ExpectInt(Compare(u"I", -1, u"\u0131", -1, IgnoreCase | ExcludeSpecialI, "Turkic I folding"), 0, "Turkic I folds to dotless i");
    Expect(Compare(u"\u0130", -1, u"i", -1, IgnoreCase | ExcludeSpecialI, "Turkic dotted I") != 0,
           "Turkic folding decomposes U+0130 first");
    Expect(Compare(u"\U00010000", -1, u"\uffff", -1, 0, "code unit order") < 0, "surrogates sort below U+FFFF by code unit");
    Expect(Compare(u"\U00010000", -1, u"\uffff", -1, CodePointOrder, "code point order") > 0, "U+10000 sorts above U+FFFF by code point");
    ExpectInt(Compare(u"\u00e9", -1, u"e\u0301", -1, Unicode32, "Unicode 3.2 filter"), 0, "Unicode 3.2 filtered equivalence");

    std::int32_t error = Success;
    ExpectInt(unorm_compare_nid_postfix(nullptr, 0, u"a", -1, 0, &error), 0, "unorm_compare(null first)");
    ExpectInt(error, IllegalArgument, "null first string is an illegal argument");
    error = Success;
    ExpectInt(unorm_compare_nid_postfix(u"a", -2, u"a", -1, 0, &error), 0, "unorm_compare(length -2)");
    ExpectInt(error, IllegalArgument, "length below -1 is an illegal argument");
    std::int32_t failing = BufferOverflow;
    ExpectInt(unorm_compare_nid_postfix(u"a", -1, u"b", -1, 0, &failing), 0, "unorm_compare with failing status");
    ExpectInt(failing, BufferOverflow, "failing status unchanged by unorm_compare");
    ExpectThrow<std::invalid_argument>([] { unorm_compare_nid_postfix(u"a", -1, u"a", -1, 0, nullptr); }, "unorm_compare(null status)");
}

}

int main() {
    TestInstances();
    TestNormalize();
    TestCompatibilityInstances();
    TestQueries();
    TestDecompositions();
    TestAppend();
    TestArguments();
    TestFiltered();
    TestCompare();
    return 0;
}
