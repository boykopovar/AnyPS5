#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKAXObject properties check failed at line %u\n", location.line());
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
    auto ax = NKWebKit::Make<NKWebKit::AccessibilityObject>();
    const void* ref = ax.Get();

    Require(WKAXObjectGetTypeID() == 132);
    Require(WKAXObjectButtonState(ref) == 0);
    Require(WKAXObjectRole(ref) == 0);

    ax->buttonState = 1;
    ax->role = 16;
    Require(WKAXObjectButtonState(ref) == 1);
    Require(WKAXObjectRole(ref) == 16);

    ax->buttonState = 2;
    ax->role = 42;
    Require(WKAXObjectButtonState(ref) == 2);
    Require(WKAXObjectRole(ref) == 42);

    RequireInvalidArgument([] { WKAXObjectButtonState(nullptr); });
    RequireInvalidArgument([] { WKAXObjectRole(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKAXObjectButtonState(frame.Get()); });
    RequireInvalidArgument([&] { WKAXObjectRole(frame.Get()); });

    return 0;
}
