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
        std::fprintf(stderr, "WKContextGetMediaSessionFocusManager check failed at line %u\n", location.line());
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
    const void* context = WKContextCreate();
    Require(context != nullptr);

    const void* manager = WKContextGetMediaSessionFocusManager(context);
    Require(manager != nullptr);
    Require(WKGetTypeID(manager) == NKWebKit::TypeID(NKWebKit::ObjectType::MediaSessionFocusManager));
    Require(WKContextGetMediaSessionFocusManager(context) == manager);

    const void* otherContext = WKContextCreate();
    Require(otherContext != nullptr);
    const void* otherManager = WKContextGetMediaSessionFocusManager(otherContext);
    Require(otherManager != nullptr && otherManager != manager);

    WKRelease(otherContext);
    WKRelease(context);

    RequireInvalidArgument([] { WKContextGetMediaSessionFocusManager(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKContextGetMediaSessionFocusManager(frame.Get()); });

    return 0;
}
