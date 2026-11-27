#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKInspector check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

unsigned Flags(const void* inspector) {
    return (WKInspectorIsConnected(inspector) ? 1u : 0u) | (WKInspectorIsAttached(inspector) ? 2u : 0u) |
           (WKInspectorIsVisible(inspector) ? 4u : 0u) | (WKInspectorIsFront(inspector) ? 8u : 0u) |
           (WKInspectorIsElementSelectionActive(inspector) ? 16u : 0u) |
           (WKInspectorIsProfilingPage(inspector) ? 32u : 0u);
}

}  // namespace

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);

    const void* inspector = WKPageGetInspector(pageRef);
    Require(inspector != nullptr);
    Require(WKGetTypeID(inspector) == WKInspectorGetTypeID());
    Require(WKPageGetInspector(pageRef) == inspector);

    Require(!WKInspectorIsConnected(inspector));
    Require(!WKInspectorIsAttached(inspector));
    Require(!WKInspectorIsVisible(inspector));
    Require(!WKInspectorIsFront(inspector));
    Require(!WKInspectorIsElementSelectionActive(inspector));
    Require(!WKInspectorIsProfilingPage(inspector));

    Require(Flags(inspector) == 0);

    WKInspectorConnect(inspector);
    Require(Flags(inspector) == 0);
    WKInspectorAttach(inspector);
    Require(Flags(inspector) == 0);
    WKInspectorShow(inspector);
    Require(Flags(inspector) == 0);
    WKInspectorShowConsole(inspector);
    Require(Flags(inspector) == 0);
    WKInspectorShowResources(inspector);
    Require(Flags(inspector) == 0);
    WKInspectorToggleElementSelection(inspector);
    Require(Flags(inspector) == 0);
    WKInspectorTogglePageProfiling(inspector);
    Require(Flags(inspector) == 0);
    WKInspectorHide(inspector);
    WKInspectorDetach(inspector);
    WKInspectorClose(inspector);
    Require(Flags(inspector) == 0);

    page->inspector->isConnected = true;
    page->inspector->isAttached = true;
    page->inspector->isVisible = true;
    page->inspector->isFront = true;
    page->inspector->isElementSelectionActive = true;
    page->inspector->isProfilingPage = true;
    Require(Flags(inspector) == 63);
    WKInspectorHide(inspector);
    Require(Flags(inspector) == 59);
    WKInspectorDetach(inspector);
    Require(Flags(inspector) == 59);
    WKInspectorClose(inspector);
    Require(Flags(inspector) == 59);
    WKInspectorToggleElementSelection(inspector);
    Require(Flags(inspector) == 59);
    WKInspectorTogglePageProfiling(inspector);
    Require(Flags(inspector) == 59);
    WKInspectorConnect(inspector);
    WKInspectorAttach(inspector);
    WKInspectorShow(inspector);
    WKInspectorShowConsole(inspector);
    WKInspectorShowResources(inspector);
    Require(Flags(inspector) == 59);

    page->isClosed = true;
    Require(WKPageGetInspector(pageRef) == nullptr);
    page->isClosed = false;
    Require(WKPageGetInspector(pageRef) == inspector);

    auto closedPage = NKWebKit::Make<NKWebKit::Page>();
    closedPage->isClosed = true;
    Require(WKPageGetInspector(closedPage.Get()) == nullptr);
    Require(!closedPage->inspector);

    auto notPage = NKWebKit::Make<NKWebKit::Inspector>();
    RequireInvalidArgument([] { WKPageGetInspector(nullptr); });
    RequireInvalidArgument([&] { WKPageGetInspector(notPage.Get()); });
    RequireInvalidArgument([&] { WKInspectorShow(pageRef); });

    WKRelease(pageRef);
    return 0;
}
