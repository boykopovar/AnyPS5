#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PageSelectionCaret check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    auto config = NKWebKit::Make<NKWebKit::PageConfiguration>();
    auto page = NKWebKit::Make<NKWebKit::Page>(std::move(config));
    const void* pageRef = page.Get();

    Require(!page->maintainsInactiveSelection);
    Require(!page->caretVisible);
    Require(!WKPageHasSelectedRange(pageRef));

    WKPageSetMaintainsInactiveSelection(pageRef, true);
    Require(page->maintainsInactiveSelection == true);

    WKPageSetCaretVisible(pageRef, true);
    Require(page->caretVisible == true);

    page->hasSelectedRange = true;
    Require(WKPageHasSelectedRange(pageRef));

    WKPageClearSelection(pageRef);
    Require(!WKPageHasSelectedRange(pageRef));

    bool threw = false;
    try {
        WKPageSetMaintainsInactiveSelection(nullptr, true);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKPageSetCaretVisible(nullptr, true);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKPageHasSelectedRange(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKPageClearSelection(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    return 0;
}
