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
        std::fprintf(stderr, "BundlePage lifecycle check failed at line %u\n", location.line());
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

    Require(!page->isClosed);
    Require(!page->isLoading);

    page->isLoading = true;
    WKBundlePageStopLoading(pageRef);
    Require(!page->isLoading);

    WKBundlePageClose(pageRef);
    Require(page->isClosed);

    auto bundle = NKWebKit::Make<NKWebKit::Bundle>();

    RequireInvalidArgument([] { WKBundlePageClose(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageClose(bundle.Get()); });
    RequireInvalidArgument([] { WKBundlePageStopLoading(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageStopLoading(bundle.Get()); });

    return 0;
}
