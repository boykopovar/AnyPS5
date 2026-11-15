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
        std::fprintf(stderr, "WKPage gap between pages controls check failed at line %u\n", location.line());
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
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    Require(WKPageGetGapBetweenPages(pageRef) == 0.0);

    WKPageSetGapBetweenPages(pageRef, 20.0);
    Require(WKPageGetGapBetweenPages(pageRef) == 20.0);

    WKPageSetGapBetweenPages(pageRef, 40.0);
    Require(WKPageGetGapBetweenPages(pageRef) == 40.0);

    RequireInvalidArgument([] { WKPageGetGapBetweenPages(nullptr); });
    RequireInvalidArgument([] { WKPageSetGapBetweenPages(nullptr, 20.0); });

    WKRelease(pageRef);
    return 0;
}
