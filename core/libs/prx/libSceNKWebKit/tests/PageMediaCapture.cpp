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
        std::fprintf(stderr, "WKPage media capture check failed at line %u\n", location.line());
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
    Require(!WKPageGetMediaCaptureEnabled(pageRef));

    WKPageSetMediaCaptureEnabled(pageRef, true);
    Require(WKPageGetMediaCaptureEnabled(pageRef));

    WKPageSetMediaCaptureEnabled(pageRef, false);
    Require(!WKPageGetMediaCaptureEnabled(pageRef));

    RequireInvalidArgument([] { WKPageGetMediaCaptureEnabled(nullptr); });
    RequireInvalidArgument([] { WKPageSetMediaCaptureEnabled(nullptr, true); });

    WKRelease(pageRef);
    return 0;
}
