#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>


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

    ExpectErr<std::invalid_argument>([] { WKPageAreScrollbarAnimationsSuppressed(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetSuppressScrollbarAnimations(nullptr, true); });
    ExpectErr<std::invalid_argument>([] { WKPageHasHorizontalScrollbar(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageHasVerticalScrollbar(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageWillHandleHorizontalScrollEvents(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageGetScrollPinningBehavior(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetScrollPinningBehavior(nullptr, 0); });
    ExpectErr<std::invalid_argument>([] { WKPageUseFixedLayout(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetUseFixedLayout(nullptr, true); });
    ExpectErr<std::invalid_argument>([] { WKPageFixedLayoutSize(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetFixedLayoutSize(nullptr, { 100.0, 100.0 }); });
    ExpectErr<std::invalid_argument>([] { WKPageGetPaginationMode(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetPaginationMode(nullptr, 0); });
    ExpectErr<std::invalid_argument>([] { WKPageGetPaginationBehavesLikeColumns(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetPaginationBehavesLikeColumns(nullptr, true); });
    ExpectErr<std::invalid_argument>([] { WKPageSetPaginationLineGridEnabled(nullptr, true); });
    ExpectErr<std::invalid_argument>([] { WKPageGetGapBetweenPages(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetGapBetweenPages(nullptr, 10.0); });
    ExpectErr<std::invalid_argument>([] { WKPageGetPageLength(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetPageLength(nullptr, 100.0); });

    WKRelease(pageRef);
    return 0;
}
