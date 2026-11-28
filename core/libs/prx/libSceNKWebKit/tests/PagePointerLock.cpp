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
        std::fprintf(stderr, "WKPage pointer lock check failed at line %u\n", location.line());
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
    const void* pageRef = page.Get();

    Require(!page->pointerLockAllowed);

    WKPageDidAllowPointerLock(pageRef);
    Require(page->pointerLockAllowed);

    WKPageDidDenyPointerLock(pageRef);
    Require(!page->pointerLockAllowed);

    RequireInvalidArgument([] { WKPageDidAllowPointerLock(nullptr); });
    RequireInvalidArgument([] { WKPageDidDenyPointerLock(nullptr); });

    auto dummyString = NKWebKit::Make<NKWebKit::String>();
    const void* dummyRef = dummyString.Get();
    RequireInvalidArgument([&] { WKPageDidAllowPointerLock(dummyRef); });
    RequireInvalidArgument([&] { WKPageDidDenyPointerLock(dummyRef); });

    return 0;
}
