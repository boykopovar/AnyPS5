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
        std::fprintf(stderr, "WKBundleNodeHandle element bounds check failed at line %u\n", location.line());
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
    auto handle = NKWebKit::Make<NKWebKit::BundleNodeHandle>();
    const void* handleRef = handle.Get();

    auto bounds = WKBundleNodeHandleGetElementBounds(handleRef);
    Require(bounds.origin.x == 0.0);
    Require(bounds.origin.y == 0.0);
    Require(bounds.size.width == 0.0);
    Require(bounds.size.height == 0.0);

    handle->elementBounds = NKWebKit::RectValue{{12.5, 24.0}, {100.5, 200.0}};
    bounds = WKBundleNodeHandleGetElementBounds(handleRef);
    Require(bounds.origin.x == 12.5);
    Require(bounds.origin.y == 24.0);
    Require(bounds.size.width == 100.5);
    Require(bounds.size.height == 200.0);

    RequireInvalidArgument([] { WKBundleNodeHandleGetElementBounds(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    const void* frameRef = frame.Get();
    RequireInvalidArgument([&] { WKBundleNodeHandleGetElementBounds(frameRef); });

    return 0;
}
