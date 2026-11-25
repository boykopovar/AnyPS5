#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKView accessibility check failed at line %u\n", location.line());
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
    auto view = NKWebKit::Make<NKWebKit::View>();
    const void* viewRef = view.Get();

    Require(WKViewAccessibilityRootObject(viewRef) == nullptr);
    Require(WKViewAccessibilityFocusedObject(viewRef) == nullptr);
    Require(WKViewAccessibilityHitTest(viewRef, NKWebKit::PointValue{ 100.0, 200.0 }) == nullptr);

    auto root = NKWebKit::Make<NKWebKit::AccessibilityObject>();
    auto focused = NKWebKit::Make<NKWebKit::AccessibilityObject>();

    view->accessibilityRootObject = root;
    view->accessibilityFocusedObject = focused;

    Require(WKViewAccessibilityRootObject(viewRef) == root.Get());
    Require(WKViewAccessibilityFocusedObject(viewRef) == focused.Get());
    Require(WKViewAccessibilityHitTest(viewRef, NKWebKit::PointValue{ 0.0, 0.0 }) == root.Get());
    Require(WKViewAccessibilityHitTest(viewRef, NKWebKit::PointValue{ 1920.0, 1080.0 }) == root.Get());

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    RequireInvalidArgument([&] { WKViewAccessibilityHitTest(viewRef, NKWebKit::PointValue{ nan, 0.0 }); });
    RequireInvalidArgument([&] { WKViewAccessibilityHitTest(viewRef, NKWebKit::PointValue{ 0.0, nan }); });
    RequireInvalidArgument([&] { WKViewAccessibilityHitTest(viewRef, NKWebKit::PointValue{ inf, 0.0 }); });
    RequireInvalidArgument([&] { WKViewAccessibilityHitTest(viewRef, NKWebKit::PointValue{ 0.0, -inf }); });

    RequireInvalidArgument([] { WKViewAccessibilityRootObject(nullptr); });
    RequireInvalidArgument([] { WKViewAccessibilityFocusedObject(nullptr); });
    RequireInvalidArgument([] { WKViewAccessibilityHitTest(nullptr, NKWebKit::PointValue{ 0.0, 0.0 }); });

    auto dummyAx = NKWebKit::Make<NKWebKit::AccessibilityObject>();
    RequireInvalidArgument([&] { WKViewAccessibilityRootObject(dummyAx.Get()); });
    RequireInvalidArgument([&] { WKViewAccessibilityFocusedObject(dummyAx.Get()); });
    RequireInvalidArgument([&] { WKViewAccessibilityHitTest(dummyAx.Get(), NKWebKit::PointValue{ 0.0, 0.0 }); });

    return 0;
}
