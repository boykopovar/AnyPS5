#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidState = 27;
constexpr std::uint8_t DefaultLtr = 0xfe;
constexpr std::int32_t Ltr = 0;
constexpr std::int32_t Rtl = 1;
constexpr std::int32_t Neutral = 3;
constexpr std::int32_t LeftToRight = 0;
constexpr std::int32_t RightToLeft = 1;
constexpr std::int32_t EuropeanNumber = 2;
constexpr std::int32_t RightToLeftArabic = 13;
constexpr std::int32_t ClassDefault = 23;

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

void ExpectVisualRun(Icu::BiDi* bidi, std::int32_t run, std::int32_t start, std::int32_t length, std::int32_t direction,
                     const char* what) {
    std::int32_t actualStart = -1;
    std::int32_t actualLength = -1;
    ExpectInt(ubidi_getVisualRun_nid_postfix(bidi, run, &actualStart, &actualLength), direction, what);
    ExpectInt(actualStart, start, what);
    ExpectInt(actualLength, length, what);
}

std::int32_t APS5_VABI MarkXRightToLeft(const void* context, std::int32_t character) {
    ++*static_cast<std::int32_t*>(const_cast<void*>(context));
    return character == u'x' ? RightToLeft : ClassDefault;
}

std::int32_t APS5_VABI DefaultClass(const void*, std::int32_t) {
    return ClassDefault;
}

void TestParagraph() {
    auto* bidi = ubidi_open_nid_postfix();
    Expect(bidi != nullptr, "ubidi_open");
    ExpectInt(ubidi_getParaLevel_nid_postfix(bidi), 0, "ubidi_getParaLevel before setPara");
    std::int32_t error = Success;
    ExpectInt(ubidi_countRuns_nid_postfix(bidi, &error), -1, "ubidi_countRuns before setPara");
    ExpectInt(error, InvalidState, "ubidi_countRuns before setPara status");

    const char16_t text[] = u"abc אבג def";
    error = Success;
    ubidi_setPara_nid_postfix(bidi, text, -1, DefaultLtr, nullptr, &error);
    ExpectInt(error, Success, "ubidi_setPara");
    ExpectInt(ubidi_getParaLevel_nid_postfix(bidi), 0, "ubidi_getParaLevel");
    ExpectInt(ubidi_countRuns_nid_postfix(bidi, &error), 3, "ubidi_countRuns");
    ExpectVisualRun(bidi, 0, 0, 4, Ltr, "ubidi_getVisualRun(0)");
    ExpectVisualRun(bidi, 1, 4, 3, Rtl, "ubidi_getVisualRun(1)");
    ExpectVisualRun(bidi, 2, 7, 4, Ltr, "ubidi_getVisualRun(2)");
    ExpectInt(ubidi_getVisualRun_nid_postfix(bidi, 3, nullptr, nullptr), Ltr, "ubidi_getVisualRun out of range");

    std::int32_t limit = -1;
    std::uint8_t level = 0xff;
    ubidi_getLogicalRun_nid_postfix(bidi, 5, &limit, &level);
    ExpectInt(limit, 7, "ubidi_getLogicalRun limit");
    ExpectInt(level, 1, "ubidi_getLogicalRun level");
    ubidi_getLogicalRun_nid_postfix(bidi, 0, &limit, &level);
    ExpectInt(limit, 4, "ubidi_getLogicalRun(0) limit");
    ExpectInt(level, 0, "ubidi_getLogicalRun(0) level");

    const char16_t hebrew[] = u"אב abc";
    ubidi_setPara_nid_postfix(bidi, hebrew, -1, DefaultLtr, nullptr, &error);
    ExpectInt(error, Success, "ubidi_setPara(RTL first)");
    ExpectInt(ubidi_getParaLevel_nid_postfix(bidi), 1, "ubidi_getParaLevel(RTL first)");
    ubidi_close_nid_postfix(bidi);
}

void TestClassCallback() {
    auto* bidi = ubidi_open_nid_postfix();
    std::int32_t calls = 0;
    Icu::BiDiClassCallback oldFunction = DefaultClass;
    const void* oldContext = &oldFunction;
    std::int32_t error = Success;
    ubidi_setClassCallback_nid_postfix(bidi, MarkXRightToLeft, &calls, &oldFunction, &oldContext, &error);
    ExpectInt(error, Success, "ubidi_setClassCallback");
    Expect(oldFunction == nullptr && oldContext == nullptr, "ubidi_setClassCallback initial callback");

    const char16_t text[] = u"axb";
    ubidi_setPara_nid_postfix(bidi, text, -1, 0, nullptr, &error);
    ExpectInt(error, Success, "ubidi_setPara with callback");
    Expect(calls >= 3, "ubidi_setPara called the class callback");
    ExpectInt(ubidi_countRuns_nid_postfix(bidi, &error), 3, "ubidi_countRuns with callback");
    ExpectVisualRun(bidi, 1, 1, 1, Rtl, "ubidi_getVisualRun with callback");

    ubidi_setClassCallback_nid_postfix(bidi, nullptr, nullptr, &oldFunction, &oldContext, &error);
    Expect(oldFunction == MarkXRightToLeft && oldContext == &calls, "ubidi_setClassCallback previous callback");
    ubidi_setPara_nid_postfix(bidi, text, -1, 0, nullptr, &error);
    ExpectInt(ubidi_countRuns_nid_postfix(bidi, &error), 1, "ubidi_countRuns after removing callback");

    error = Success;
    ubidi_setClassCallback_nid_postfix(nullptr, MarkXRightToLeft, &calls, nullptr, nullptr, &error);
    ExpectInt(error, IllegalArgument, "ubidi_setClassCallback(null)");
    ubidi_setClassCallback_nid_postfix(bidi, MarkXRightToLeft, &calls, nullptr, nullptr, nullptr);
    ubidi_close_nid_postfix(bidi);
}

void TestStatelessHelpers() {
    ExpectInt(ubidi_getBaseDirection_nid_postfix(u"123 אabc", -1), Rtl, "ubidi_getBaseDirection(Hebrew)");
    ExpectInt(ubidi_getBaseDirection_nid_postfix(u"abc", -1), Ltr, "ubidi_getBaseDirection(Latin)");
    ExpectInt(ubidi_getBaseDirection_nid_postfix(u"123 !", -1), Neutral, "ubidi_getBaseDirection(neutral)");
    ExpectInt(ubidi_getBaseDirection_nid_postfix(u"א", 0), Neutral, "ubidi_getBaseDirection(empty)");
    ExpectInt(ubidi_getBaseDirection_nid_postfix(nullptr, -1), Neutral, "ubidi_getBaseDirection(null)");

    const std::uint8_t levels[] = {0, 0, 1, 1, 1, 0};
    std::int32_t map[6] = {};
    ubidi_reorderVisual_nid_postfix(levels, 6, map);
    const std::int32_t expected[] = {0, 1, 4, 3, 2, 5};
    for (int i = 0; i < 6; ++i) ExpectInt(map[i], expected[i], "ubidi_reorderVisual");

    ExpectInt(ubidi_getClass_nid_postfix(u'A'), LeftToRight, "ubidi_getClass(A)");
    ExpectInt(ubidi_getClass_nid_postfix(0x05D0), RightToLeft, "ubidi_getClass(U+05D0)");
    ExpectInt(ubidi_getClass_nid_postfix(u'7'), EuropeanNumber, "ubidi_getClass(7)");
    ExpectInt(ubidi_getClass_nid_postfix(0x0627), RightToLeftArabic, "ubidi_getClass(U+0627)");
}

void TestArguments() {
    std::int32_t error = Success;
    ubidi_setPara_nid_postfix(nullptr, u"a", -1, 0, nullptr, &error);
    ExpectInt(error, IllegalArgument, "ubidi_setPara(null)");
    ExpectThrow<std::invalid_argument>([] { ubidi_getLogicalRun_nid_postfix(nullptr, 0, nullptr, nullptr); },
                                       "ubidi_getLogicalRun(null)");
    ubidi_close_nid_postfix(nullptr);
}

}

int main() {
    TestParagraph();
    TestClassCallback();
    TestStatelessHelpers();
    TestArguments();
    return 0;
}
