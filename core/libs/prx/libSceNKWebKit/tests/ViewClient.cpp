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
        std::fprintf(stderr, "ViewClient check failed at line %u\n", location.line());
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
    const void* viewRef = view.Get();

    Require(view->viewClient == nullptr);
    Require(view->viewPopupMenuClient == nullptr);
    Require(view->lastPopupMenuValue == -1);

    int dummyClient = 42;
    WKViewSetViewClient(viewRef, &dummyClient);
    Require(view->viewClient == &dummyClient);

    WKViewSetViewClient(viewRef, nullptr);
    Require(view->viewClient == nullptr);

    int dummyPopupClient = 84;
    WKViewSetViewPopupMenuClient(viewRef, &dummyPopupClient);
    Require(view->viewPopupMenuClient == &dummyPopupClient);

    WKViewSetViewPopupMenuClient(viewRef, nullptr);
    Require(view->viewPopupMenuClient == nullptr);

    WKViewValueChangedForPopupMenu(viewRef, 0);
    Require(view->lastPopupMenuValue == 0);

    WKViewValueChangedForPopupMenu(viewRef, 5);
    Require(view->lastPopupMenuValue == 5);

    WKViewValueChangedForPopupMenu(viewRef, -1);
    Require(view->lastPopupMenuValue == -1);

    auto badObj = NKWebKit::Make<NKWebKit::Array>();
    RequireInvalidArgument([&] { WKViewSetViewClient(badObj.Get(), &dummyClient); });
    RequireInvalidArgument([&] { WKViewSetViewPopupMenuClient(badObj.Get(), &dummyPopupClient); });
    RequireInvalidArgument([&] { WKViewValueChangedForPopupMenu(badObj.Get(), 1); });

    RequireInvalidArgument([&] { WKViewSetViewClient(nullptr, &dummyClient); });
    RequireInvalidArgument([&] { WKViewSetViewPopupMenuClient(nullptr, &dummyPopupClient); });
    RequireInvalidArgument([] { WKViewValueChangedForPopupMenu(nullptr, 1); });

    return 0;
}
