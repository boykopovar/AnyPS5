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
        std::fprintf(stderr, "WKBundlePage zoom check failed at line %u\n", location.line());
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

    Require(WKBundlePageGetPageZoomFactor(pageRef) == 1.0);
    Require(WKBundlePageGetTextZoomFactor(pageRef) == 1.0);

    WKBundlePageSetPageZoomFactor(pageRef, 1.5);
    Require(WKBundlePageGetPageZoomFactor(pageRef) == 1.5);

    WKBundlePageSetTextZoomFactor(pageRef, 2.25);
    Require(WKBundlePageGetTextZoomFactor(pageRef) == 2.25);

    WKBundlePageSetPageZoomFactor(pageRef, 0.0);
    Require(WKBundlePageGetPageZoomFactor(pageRef) == 1.5);

    WKBundlePageSetPageZoomFactor(pageRef, -1.0);
    Require(WKBundlePageGetPageZoomFactor(pageRef) == 1.5);

    WKBundlePageSetTextZoomFactor(pageRef, 0.0);
    Require(WKBundlePageGetTextZoomFactor(pageRef) == 2.25);

    WKBundlePageSetTextZoomFactor(pageRef, -0.5);
    Require(WKBundlePageGetTextZoomFactor(pageRef) == 2.25);

    WKBundlePageSetPageZoomFactor(pageRef, 0.75);
    Require(WKBundlePageGetPageZoomFactor(pageRef) == 0.75);

    WKBundlePageSetTextZoomFactor(pageRef, 1.0);
    Require(WKBundlePageGetTextZoomFactor(pageRef) == 1.0);

    RequireInvalidArgument([] { WKBundlePageGetPageZoomFactor(nullptr); });
    RequireInvalidArgument([] { WKBundlePageGetTextZoomFactor(nullptr); });
    RequireInvalidArgument([] { WKBundlePageSetPageZoomFactor(nullptr, 1.5); });
    RequireInvalidArgument([] { WKBundlePageSetTextZoomFactor(nullptr, 1.5); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKBundlePageGetPageZoomFactor(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageGetTextZoomFactor(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageSetPageZoomFactor(frame.Get(), 1.5); });
    RequireInvalidArgument([&] { WKBundlePageSetTextZoomFactor(frame.Get(), 1.5); });

    return 0;
}
