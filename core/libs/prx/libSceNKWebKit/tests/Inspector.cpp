#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

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
    ExpectErr<std::invalid_argument>([] { WKPageGetInspector(nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKPageGetInspector(notPage.Get()); });
    ExpectErr<std::invalid_argument>([&] { WKInspectorShow(pageRef); });

    WKRelease(pageRef);
    return 0;
}
