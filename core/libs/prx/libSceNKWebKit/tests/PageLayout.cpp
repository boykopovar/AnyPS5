#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage layout check failed at line %u\n", location.line());
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

bool SizeIs(const void* pageRef, double width, double height) {
    const auto size = WKPageFixedLayoutSize(pageRef);
    return size.width == width && size.height == height;
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);

    Require(!WKPageAreScrollbarAnimationsSuppressed(pageRef));
    Require(!WKPageHasHorizontalScrollbar(pageRef));
    Require(!WKPageHasVerticalScrollbar(pageRef));
    Require(!WKPageWillHandleHorizontalScrollEvents(pageRef));
    Require(WKPageGetScrollPinningBehavior(pageRef) == 0);
    Require(!WKPageUseFixedLayout(pageRef));
    Require(WKPageFixedLayoutSize(pageRef).width == 0.0);
    Require(WKPageFixedLayoutSize(pageRef).height == 0.0);
    Require(WKPageGetPaginationMode(pageRef) == 0);
    Require(!WKPageGetPaginationBehavesLikeColumns(pageRef));
    Require(WKPageGetGapBetweenPages(pageRef) == 0.0);
    Require(WKPageGetPageLength(pageRef) == 0.0);
    Require(!WKPageGetPaginationLineGridEnabled(pageRef));

    WKPageSetSuppressScrollbarAnimations(pageRef, true);
    Require(WKPageAreScrollbarAnimationsSuppressed(pageRef));
    WKPageSetSuppressScrollbarAnimations(pageRef, false);
    Require(!WKPageAreScrollbarAnimationsSuppressed(pageRef));

    page->hasHorizontalScrollbar = true;
    Require(WKPageHasHorizontalScrollbar(pageRef));
    page->hasHorizontalScrollbar = false;
    Require(!WKPageHasHorizontalScrollbar(pageRef));

    page->hasVerticalScrollbar = true;
    Require(WKPageHasVerticalScrollbar(pageRef));
    page->hasVerticalScrollbar = false;
    Require(!WKPageHasVerticalScrollbar(pageRef));

    page->willHandleHorizontalScrollEvents = true;
    Require(WKPageWillHandleHorizontalScrollEvents(pageRef));
    page->willHandleHorizontalScrollEvents = false;
    Require(!WKPageWillHandleHorizontalScrollEvents(pageRef));

    WKPageSetScrollPinningBehavior(pageRef, 1);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 1);
    WKPageSetScrollPinningBehavior(pageRef, 2);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 2);
    WKPageSetScrollPinningBehavior(pageRef, 3);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 0);
    WKPageSetScrollPinningBehavior(pageRef, 1);
    WKPageSetScrollPinningBehavior(pageRef, 0xFFFFFFFFu);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 0);

    WKPageSetUseFixedLayout(pageRef, true);
    Require(WKPageUseFixedLayout(pageRef));
    WKPageSetFixedLayoutSize(pageRef, { 1920.0, 1080.0 });
    Require(SizeIs(pageRef, 1920.0, 1080.0));
    WKPageSetUseFixedLayout(pageRef, true);
    Require(SizeIs(pageRef, 1920.0, 1080.0));
    WKPageSetUseFixedLayout(pageRef, false);
    Require(!WKPageUseFixedLayout(pageRef));
    Require(SizeIs(pageRef, 0.0, 0.0));
    WKPageSetFixedLayoutSize(pageRef, { -100.0, -50.0 });
    Require(SizeIs(pageRef, -100.0, -50.0));
    WKPageSetUseFixedLayout(pageRef, false);
    Require(SizeIs(pageRef, -100.0, -50.0));

    WKPageSetFixedLayoutSize(pageRef, { 1.7, 2.9 });
    Require(SizeIs(pageRef, 1.0, 2.0));
    WKPageSetFixedLayoutSize(pageRef, { -1.7, -2.9 });
    Require(SizeIs(pageRef, -1.0, -2.0));
    WKPageSetFixedLayoutSize(pageRef, { -0.5, 0.5 });
    Require(SizeIs(pageRef, 0.0, 0.0));
    Require(!std::signbit(WKPageFixedLayoutSize(pageRef).width));
    WKPageSetFixedLayoutSize(pageRef, { 2147483647.9, -2147483648.9 });
    Require(SizeIs(pageRef, 2147483647.0, -2147483648.0));
    WKPageSetFixedLayoutSize(pageRef, { 2147483648.0, -2147483649.0 });
    Require(SizeIs(pageRef, -2147483648.0, -2147483648.0));
    WKPageSetFixedLayoutSize(pageRef, { 1e300, -1e300 });
    Require(SizeIs(pageRef, -2147483648.0, -2147483648.0));
    WKPageSetFixedLayoutSize(pageRef, { std::nan(""), std::numeric_limits<double>::infinity() });
    Require(SizeIs(pageRef, -2147483648.0, -2147483648.0));

    WKPageSetPaginationMode(pageRef, 1);
    Require(WKPageGetPaginationMode(pageRef) == 1);
    WKPageSetPaginationMode(pageRef, 4);
    Require(WKPageGetPaginationMode(pageRef) == 4);
    WKPageSetPaginationMode(pageRef, 5);
    Require(WKPageGetPaginationMode(pageRef) == 4);
    WKPageSetPaginationMode(pageRef, 0xFFFFFFFFu);
    Require(WKPageGetPaginationMode(pageRef) == 4);
    WKPageSetPaginationMode(pageRef, 2);
    Require(WKPageGetPaginationMode(pageRef) == 2);
    WKPageSetPaginationMode(pageRef, 0);
    Require(WKPageGetPaginationMode(pageRef) == 0);

    WKPageSetPaginationBehavesLikeColumns(pageRef, true);
    Require(WKPageGetPaginationBehavesLikeColumns(pageRef));
    WKPageSetPaginationBehavesLikeColumns(pageRef, false);
    Require(!WKPageGetPaginationBehavesLikeColumns(pageRef));

    WKPageSetPaginationLineGridEnabled(pageRef, true);
    Require(!WKPageGetPaginationLineGridEnabled(pageRef));
    WKPageSetPaginationLineGridEnabled(pageRef, false);
    Require(!WKPageGetPaginationLineGridEnabled(pageRef));
    WKPageSetPaginationLineGridEnabled(nullptr, true);
    Require(!WKPageGetPaginationLineGridEnabled(nullptr));

    WKPageSetGapBetweenPages(pageRef, 32.0);
    Require(WKPageGetGapBetweenPages(pageRef) == 32.0);
    WKPageSetGapBetweenPages(pageRef, -10.0);
    Require(WKPageGetGapBetweenPages(pageRef) == -10.0);

    WKPageSetPageLength(pageRef, 800.0);
    Require(WKPageGetPageLength(pageRef) == 800.0);
    WKPageSetPageLength(pageRef, -50.0);
    Require(WKPageGetPageLength(pageRef) == -50.0);

    RequireInvalidArgument([] { WKPageAreScrollbarAnimationsSuppressed(nullptr); });
    RequireInvalidArgument([] { WKPageSetSuppressScrollbarAnimations(nullptr, true); });
    RequireInvalidArgument([] { WKPageHasHorizontalScrollbar(nullptr); });
    RequireInvalidArgument([] { WKPageHasVerticalScrollbar(nullptr); });
    RequireInvalidArgument([] { WKPageWillHandleHorizontalScrollEvents(nullptr); });
    RequireInvalidArgument([] { WKPageGetScrollPinningBehavior(nullptr); });
    RequireInvalidArgument([] { WKPageSetScrollPinningBehavior(nullptr, 0); });
    RequireInvalidArgument([] { WKPageUseFixedLayout(nullptr); });
    RequireInvalidArgument([] { WKPageSetUseFixedLayout(nullptr, true); });
    RequireInvalidArgument([] { WKPageFixedLayoutSize(nullptr); });
    RequireInvalidArgument([] { WKPageSetFixedLayoutSize(nullptr, { 100.0, 100.0 }); });
    RequireInvalidArgument([] { WKPageGetPaginationMode(nullptr); });
    RequireInvalidArgument([] { WKPageSetPaginationMode(nullptr, 0); });
    RequireInvalidArgument([] { WKPageGetPaginationBehavesLikeColumns(nullptr); });
    RequireInvalidArgument([] { WKPageSetPaginationBehavesLikeColumns(nullptr, true); });
    RequireInvalidArgument([] { WKPageGetGapBetweenPages(nullptr); });
    RequireInvalidArgument([] { WKPageSetGapBetweenPages(nullptr, 10.0); });
    RequireInvalidArgument([] { WKPageGetPageLength(nullptr); });
    RequireInvalidArgument([] { WKPageSetPageLength(nullptr, 100.0); });

    WKRelease(pageRef);
    return 0;
}
