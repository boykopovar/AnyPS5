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
        std::fprintf(stderr, "WKBundlePage painted objects counter threshold check failed at line %u\n", location.line());
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
    auto page = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* pageRef = page.Get();

    Require(page->paintedObjectsCounterThreshold == 0);

    WKBundlePageSetPaintedObjectsCounterThreshold(pageRef, 42);
    Require(page->paintedObjectsCounterThreshold == 42);

    WKBundlePageSetPaintedObjectsCounterThreshold(pageRef, 0xFFFF'FFFF'FFFF'FFFFULL);
    Require(page->paintedObjectsCounterThreshold == 0xFFFF'FFFF'FFFF'FFFFULL);

    WKBundlePageSetPaintedObjectsCounterThreshold(pageRef, 0);
    Require(page->paintedObjectsCounterThreshold == 0);

    RequireInvalidArgument([&] { WKBundlePageSetPaintedObjectsCounterThreshold(nullptr, 100); });

    auto stringObj = NKWebKit::Make<NKWebKit::String>();
    RequireInvalidArgument([&] { WKBundlePageSetPaintedObjectsCounterThreshold(stringObj.Get(), 100); });

    return 0;
}
