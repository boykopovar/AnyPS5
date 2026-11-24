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
        std::fprintf(stderr, "WKBundlePage repaint check failed at line %u\n", location.line());
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

    Require(!WKBundlePageIsTrackingRepaints(pageRef));

    const void* rects = WKBundlePageCopyTrackedRepaintRects(pageRef);
    Require(rects != nullptr);
    Require(WKGetTypeID(rects) == WKArrayGetTypeID());
    Require(WKArrayGetSize(rects) == 0);
    WKRelease(rects);

    WKBundlePageSetTracksRepaints(pageRef, true);
    Require(WKBundlePageIsTrackingRepaints(pageRef));

    auto rectArray = NKWebKit::Make<NKWebKit::Array>();
    page->trackedRepaintRects = rectArray;

    WKBundlePageResetTrackedRepaints(pageRef);
    Require(page->trackedRepaintRects == nullptr);

    page->trackedRepaintRects = rectArray;
    WKBundlePageSetTracksRepaints(pageRef, false);
    Require(!WKBundlePageIsTrackingRepaints(pageRef));
    Require(page->trackedRepaintRects == nullptr);

    RequireInvalidArgument([] { WKBundlePageIsTrackingRepaints(nullptr); });
    RequireInvalidArgument([] { WKBundlePageSetTracksRepaints(nullptr, true); });
    RequireInvalidArgument([] { WKBundlePageResetTrackedRepaints(nullptr); });
    RequireInvalidArgument([] { WKBundlePageCopyTrackedRepaintRects(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKBundlePageIsTrackingRepaints(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageSetTracksRepaints(frame.Get(), true); });
    RequireInvalidArgument([&] { WKBundlePageResetTrackedRepaints(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageCopyTrackedRepaintRects(frame.Get()); });

    return 0;
}
