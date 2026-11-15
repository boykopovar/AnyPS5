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

    WKInspectorConnect(inspector);
    WKInspectorAttach(inspector);
    WKInspectorShow(inspector);
    WKInspectorToggleElementSelection(inspector);
    WKInspectorTogglePageProfiling(inspector);

    Require(WKInspectorIsConnected(inspector));
    Require(WKInspectorIsAttached(inspector));
    Require(WKInspectorIsVisible(inspector));
    Require(WKInspectorIsFront(inspector));
    Require(WKInspectorIsElementSelectionActive(inspector));
    Require(WKInspectorIsProfilingPage(inspector));

    WKInspectorHide(inspector);
    Require(!WKInspectorIsVisible(inspector));
    Require(!WKInspectorIsFront(inspector));

    WKInspectorShowConsole(inspector);
    Require(WKInspectorIsVisible(inspector));
    Require(WKInspectorIsFront(inspector));

    WKInspectorClose(inspector);
    Require(!WKInspectorIsConnected(inspector));
    Require(!WKInspectorIsAttached(inspector));
    Require(!WKInspectorIsVisible(inspector));
    Require(!WKInspectorIsFront(inspector));
    Require(!WKInspectorIsElementSelectionActive(inspector));
    Require(!WKInspectorIsProfilingPage(inspector));

    auto notPage = NKWebKit::Make<NKWebKit::Inspector>();
    RequireInvalidArgument([] { WKPageGetInspector(nullptr); });
    RequireInvalidArgument([&] { WKPageGetInspector(notPage.Get()); });
    RequireInvalidArgument([&] { WKInspectorShow(pageRef); });

    WKRelease(pageRef);
    return 0;
}
