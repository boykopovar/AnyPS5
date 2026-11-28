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
        std::fprintf(stderr, "WKBundlePage local data check failed at line %u\n", location.line());
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

    auto url = NKWebKit::Make<NKWebKit::URL>("https://example.test/local");
    const void* urlRef = url.Get();

    Require(!WKBundlePageHasLocalDataForURL(pageRef, urlRef));

    page->hasLocalDataForURL = true;
    Require(WKBundlePageHasLocalDataForURL(pageRef, urlRef));

    page->hasLocalDataForURL = false;
    Require(!WKBundlePageHasLocalDataForURL(pageRef, urlRef));

    RequireInvalidArgument([&] { WKBundlePageHasLocalDataForURL(nullptr, urlRef); });
    RequireInvalidArgument([&] { WKBundlePageHasLocalDataForURL(pageRef, nullptr); });
    RequireInvalidArgument([] { WKBundlePageHasLocalDataForURL(nullptr, nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    const void* frameRef = frame.Get();

    RequireInvalidArgument([&] { WKBundlePageHasLocalDataForURL(frameRef, urlRef); });
    RequireInvalidArgument([&] { WKBundlePageHasLocalDataForURL(pageRef, frameRef); });

    return 0;
}
