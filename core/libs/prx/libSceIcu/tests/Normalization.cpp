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

}

int main() {
    TestInstances();
    TestNormalize();
    TestQueries();
    TestDecompositions();
    TestAppend();
    TestArguments();
    return 0;
}
