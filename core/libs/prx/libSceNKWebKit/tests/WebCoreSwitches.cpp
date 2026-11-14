#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace {

using NKWebKit::WebCoreAbi::FontCascadeCodePath;

static_assert(std::is_same_v<std::underlying_type_t<FontCascadeCodePath>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(FontCascadeCodePath::Auto) == 0);
static_assert(static_cast<std::uint8_t>(FontCascadeCodePath::Simple) == 1);
static_assert(static_cast<std::uint8_t>(FontCascadeCodePath::Complex) == 2);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore switches check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

void TestAccessibility() {
    Require(!NKWebKit::AccessibilityEnabled());
    Require(!NKWebKit::AccessibilityEnhancedUserInterfaceEnabled());

    _ZN7WebCore13AXObjectCache19enableAccessibilityEv();
    Require(NKWebKit::AccessibilityEnabled());
    _ZN7WebCore13AXObjectCache19enableAccessibilityEv();
    Require(NKWebKit::AccessibilityEnabled());
    Require(!NKWebKit::AccessibilityEnhancedUserInterfaceEnabled());

    _ZN7WebCore13AXObjectCache20disableAccessibilityEv();
    Require(!NKWebKit::AccessibilityEnabled());

    _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(true);
    Require(NKWebKit::AccessibilityEnhancedUserInterfaceEnabled());
    Require(!NKWebKit::AccessibilityEnabled());

    _ZN7WebCore13AXObjectCache19enableAccessibilityEv();
    _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(false);
    Require(!NKWebKit::AccessibilityEnhancedUserInterfaceEnabled());
    Require(NKWebKit::AccessibilityEnabled());

    _ZN7WebCore13AXObjectCache20disableAccessibilityEv();
    Require(!NKWebKit::AccessibilityEnabled());
}

void TestCodePath() {
    Require(NKWebKit::CurrentFontCascadeCodePath() == FontCascadeCodePath::Auto);
    const FontCascadeCodePath paths[] = {
        FontCascadeCodePath::Simple,
        FontCascadeCodePath::Complex,
        FontCascadeCodePath::SimpleWithGlyphOverflow,
        FontCascadeCodePath::Auto,
    };
    for (const auto path : paths) {
        _ZN7WebCore11FontCascade11setCodePathENS0_8CodePathE(path);
        Require(NKWebKit::CurrentFontCascadeCodePath() == path);
    }
}

void TestSmoothing() {
    Require(_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(false);
    Require(!_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(false);
    Require(!_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(true);
    Require(_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
}

}  // namespace

int main() {
    const auto stubsBefore = NotImplementedCalls_nid_no_patch();
    TestAccessibility();
    TestCodePath();
    TestSmoothing();
    Require(NotImplementedCalls_nid_no_patch() == stubsBefore);
    std::puts("WebCore switches tests passed");
    return 0;
}
