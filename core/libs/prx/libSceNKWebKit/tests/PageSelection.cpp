#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    auto pageObj = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(pageObj);
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);

    Require(!page->maintainsInactiveSelection);
    Require(!page->caretVisible);
    Require(!page->focusedNodeBlurred);
    Require(!page->selectionCleared);
    Require(!page->selectionCentered);

    WKPageBlurFocusedNode(pageRef);
    Require(page->focusedNodeBlurred);

    WKPageCenterSelectionInVisibleArea(pageRef);
    Require(page->selectionCentered);

    WKPageClearSelection(pageRef);
    Require(page->selectionCleared);

    WKPageSetCaretVisible(pageRef, true);
    Require(page->caretVisible);
    WKPageSetCaretVisible(pageRef, false);
    Require(!page->caretVisible);

    WKPageSetMaintainsInactiveSelection(pageRef, true);
    Require(page->maintainsInactiveSelection);
    WKPageSetMaintainsInactiveSelection(pageRef, false);
    Require(!page->maintainsInactiveSelection);

    const void* wrongType = WKStringCreateWithUTF8CString("not_a_page");

    RequireThrows([&] { WKPageBlurFocusedNode(nullptr); });
    RequireThrows([&] { WKPageBlurFocusedNode(wrongType); });
    RequireThrows([&] { WKPageCenterSelectionInVisibleArea(nullptr); });
    RequireThrows([&] { WKPageCenterSelectionInVisibleArea(wrongType); });
    RequireThrows([&] { WKPageClearSelection(nullptr); });
    RequireThrows([&] { WKPageClearSelection(wrongType); });
    RequireThrows([&] { WKPageSetCaretVisible(nullptr, true); });
    RequireThrows([&] { WKPageSetCaretVisible(wrongType, true); });
    RequireThrows([&] { WKPageSetMaintainsInactiveSelection(nullptr, true); });
    RequireThrows([&] { WKPageSetMaintainsInactiveSelection(wrongType, true); });

    WKRelease(wrongType);
    WKRelease(pageRef);
    return 0;
}
