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
        std::fprintf(stderr, "WKBundlePage full-screen check failed at line %u\n", location.line());
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

    Require(!page->isFullScreen);

    WKBundlePageWillEnterFullScreen(pageRef);
    Require(!page->isFullScreen);

    WKBundlePageDidEnterFullScreen(pageRef);
    Require(page->isFullScreen);

    WKBundlePageWillExitFullScreen(pageRef);
    Require(page->isFullScreen);

    WKBundlePageDidExitFullScreen(pageRef);
    Require(!page->isFullScreen);

    RequireInvalidArgument([] { WKBundlePageWillEnterFullScreen(nullptr); });
    RequireInvalidArgument([] { WKBundlePageDidEnterFullScreen(nullptr); });
    RequireInvalidArgument([] { WKBundlePageWillExitFullScreen(nullptr); });
    RequireInvalidArgument([] { WKBundlePageDidExitFullScreen(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    const void* frameRef = frame.Get();

    RequireInvalidArgument([&] { WKBundlePageWillEnterFullScreen(frameRef); });
    RequireInvalidArgument([&] { WKBundlePageDidEnterFullScreen(frameRef); });
    RequireInvalidArgument([&] { WKBundlePageWillExitFullScreen(frameRef); });
    RequireInvalidArgument([&] { WKBundlePageDidExitFullScreen(frameRef); });

    return 0;
}
