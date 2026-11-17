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
    Require(!page->paginationLineGridEnabled);

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

    WKPageSetUseFixedLayout(pageRef, true);
    Require(WKPageUseFixedLayout(pageRef));
    WKPageSetUseFixedLayout(pageRef, false);
    Require(!WKPageUseFixedLayout(pageRef));

    WKPageSetFixedLayoutSize(pageRef, { 1920.0, 1080.0 });
    Require(WKPageFixedLayoutSize(pageRef).width == 1920.0);
    Require(WKPageFixedLayoutSize(pageRef).height == 1080.0);
    WKPageSetFixedLayoutSize(pageRef, { -100.0, -50.0 });
    Require(WKPageFixedLayoutSize(pageRef).width == -100.0);
    Require(WKPageFixedLayoutSize(pageRef).height == -50.0);

    WKPageSetPaginationMode(pageRef, 1);
    Require(WKPageGetPaginationMode(pageRef) == 1);
    WKPageSetPaginationMode(pageRef, 4);
    Require(WKPageGetPaginationMode(pageRef) == 4);

    WKPageSetPaginationBehavesLikeColumns(pageRef, true);
    Require(WKPageGetPaginationBehavesLikeColumns(pageRef));
    WKPageSetPaginationBehavesLikeColumns(pageRef, false);
    Require(!WKPageGetPaginationBehavesLikeColumns(pageRef));

    WKPageSetPaginationLineGridEnabled(pageRef, true);
    Require(page->paginationLineGridEnabled);
    WKPageSetPaginationLineGridEnabled(pageRef, false);
    Require(!page->paginationLineGridEnabled);

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
    RequireInvalidArgument([] { WKPageSetPaginationLineGridEnabled(nullptr, true); });
    RequireInvalidArgument([] { WKPageGetGapBetweenPages(nullptr); });
    RequireInvalidArgument([] { WKPageSetGapBetweenPages(nullptr, 10.0); });
    RequireInvalidArgument([] { WKPageGetPageLength(nullptr); });
    RequireInvalidArgument([] { WKPageSetPageLength(nullptr, 100.0); });

    WKRelease(pageRef);
    return 0;
}
