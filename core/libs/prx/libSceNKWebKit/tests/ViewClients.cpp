#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ViewClients check failed at line %u\n", location.line());
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

}

int main() {
    auto view = NKWebKit::Make<NKWebKit::View>();
    const void* viewRef = NKWebKit::ToRef(view);

    Require(!view->clientConfigured);
    Require(!view->popupMenuClientConfigured);
    Require(view->popupMenuSelectedIndex == -1);

    const void* dummyClient = viewRef;
    WKViewSetViewClient(viewRef, dummyClient);
    Require(view->clientConfigured);
    WKViewSetViewClient(viewRef, nullptr);
    Require(!view->clientConfigured);

    WKViewSetViewPopupMenuClient(viewRef, dummyClient);
    Require(view->popupMenuClientConfigured);
    WKViewSetViewPopupMenuClient(viewRef, nullptr);
    Require(!view->popupMenuClientConfigured);

    WKViewValueChangedForPopupMenu(viewRef, 3);
    Require(view->popupMenuSelectedIndex == 3);
    WKViewValueChangedForPopupMenu(viewRef, -1);
    Require(view->popupMenuSelectedIndex == -1);

    const void* punchHoles = WKViewGetPunchHoles(viewRef);
    Require(punchHoles != nullptr);
    Require(WKArrayGetSize(punchHoles) == 0);

    auto wrongType = NKWebKit::Make<NKWebKit::String>();
    const void* wrongRef = NKWebKit::ToRef(wrongType);

    RequireInvalidArgument([&] { WKViewGetPunchHoles(nullptr); });
    RequireInvalidArgument([&] { WKViewGetPunchHoles(wrongRef); });

    RequireInvalidArgument([&] { WKViewSetViewClient(nullptr, dummyClient); });
    RequireInvalidArgument([&] { WKViewSetViewClient(wrongRef, dummyClient); });

    RequireInvalidArgument([&] { WKViewSetViewPopupMenuClient(nullptr, dummyClient); });
    RequireInvalidArgument([&] { WKViewSetViewPopupMenuClient(wrongRef, dummyClient); });

    RequireInvalidArgument([&] { WKViewValueChangedForPopupMenu(nullptr, 0); });
    RequireInvalidArgument([&] { WKViewValueChangedForPopupMenu(wrongRef, 0); });

    return 0;
}
