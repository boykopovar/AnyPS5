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
        std::fprintf(stderr, "WKBundlePage state check failed at line %u\n", location.line());
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

    Require(WKBundlePageGetBackingScaleFactor(pageRef) == 1.0);
    Require(!WKBundlePageIsControlledByAutomation(pageRef));
    Require(!WKBundlePageIsSuspended(pageRef));
    Require(!WKBundlePageIsUsingEphemeralSession(pageRef));
    Require(WKBundlePageGetRenderTreeSize(pageRef) == 0);
    Require(!WKBundlePageHasComposition(pageRef));

    page->backingScaleFactor = 2.0;
    page->isControlledByAutomation = true;
    page->isSuspended = true;
    page->usesEphemeralSession = true;
    page->renderTreeSize = 0x1'0000'0001ULL;
    page->hasComposition = true;

    Require(WKBundlePageGetBackingScaleFactor(pageRef) == 2.0);
    Require(WKBundlePageIsControlledByAutomation(pageRef));
    Require(WKBundlePageIsSuspended(pageRef));
    Require(WKBundlePageIsUsingEphemeralSession(pageRef));
    Require(WKBundlePageGetRenderTreeSize(pageRef) == 0x1'0000'0001ULL);
    Require(WKBundlePageHasComposition(pageRef));

    RequireInvalidArgument([] { WKBundlePageGetBackingScaleFactor(nullptr); });
    RequireInvalidArgument([] { WKBundlePageIsControlledByAutomation(nullptr); });
    RequireInvalidArgument([] { WKBundlePageIsSuspended(nullptr); });
    RequireInvalidArgument([] { WKBundlePageIsUsingEphemeralSession(nullptr); });
    RequireInvalidArgument([] { WKBundlePageGetRenderTreeSize(nullptr); });
    RequireInvalidArgument([] { WKBundlePageHasComposition(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKBundlePageGetBackingScaleFactor(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageIsControlledByAutomation(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageIsSuspended(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageIsUsingEphemeralSession(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageGetRenderTreeSize(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageHasComposition(frame.Get()); });

    return 0;
}
