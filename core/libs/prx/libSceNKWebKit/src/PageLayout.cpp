#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

namespace {

double TruncatedToInt(double value) {
    if (!(value > -2147483649.0 && value < 2147483648.0)) return -2147483648.0;
    return static_cast<double>(static_cast<int>(value));
}

}

extern "C" {

bool APS5_VABI WKPageAreScrollbarAnimationsSuppressed(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->suppressScrollbarAnimations;
}

void APS5_VABI WKPageSetSuppressScrollbarAnimations(const void* pageRef, bool suppressAnimations) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->suppressScrollbarAnimations = suppressAnimations;
}

bool APS5_VABI WKPageHasHorizontalScrollbar(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->hasHorizontalScrollbar;
}

bool APS5_VABI WKPageHasVerticalScrollbar(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->hasVerticalScrollbar;
}

bool APS5_VABI WKPageWillHandleHorizontalScrollEvents(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->willHandleHorizontalScrollEvents;
}

std::uint32_t APS5_VABI WKPageGetScrollPinningBehavior(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->scrollPinningBehavior;
}

void APS5_VABI WKPageSetScrollPinningBehavior(const void* pageRef, std::uint32_t pinning) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->scrollPinningBehavior = pinning <= 2 ? pinning : 0;
}

bool APS5_VABI WKPageUseFixedLayout(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->useFixedLayout;
}

void APS5_VABI WKPageSetUseFixedLayout(const void* pageRef, bool fixed) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->useFixedLayout == fixed) return;
    page->useFixedLayout = fixed;
    if (!fixed) page->fixedLayoutSize = NKWebKit::SizeValue{0.0, 0.0};
}

NKWebKit::SizeValue APS5_VABI WKPageFixedLayoutSize(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->fixedLayoutSize;
}

void APS5_VABI WKPageSetFixedLayoutSize(const void* pageRef, NKWebKit::SizeValue size) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->fixedLayoutSize = NKWebKit::SizeValue{TruncatedToInt(size.width), TruncatedToInt(size.height)};
}

std::uint32_t APS5_VABI WKPageGetPaginationMode(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->paginationMode;
}

void APS5_VABI WKPageSetPaginationMode(const void* pageRef, std::uint32_t paginationMode) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (paginationMode > 4) return;
    page->paginationMode = paginationMode;
}

bool APS5_VABI WKPageGetPaginationBehavesLikeColumns(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->paginationBehavesLikeColumns;
}

void APS5_VABI WKPageSetPaginationBehavesLikeColumns(const void* pageRef, bool behavesLikeColumns) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->paginationBehavesLikeColumns = behavesLikeColumns;
}

bool APS5_VABI WKPageGetPaginationLineGridEnabled(const void*) {
    return false;
}

void APS5_VABI WKPageSetPaginationLineGridEnabled(const void*, bool) {
}

double APS5_VABI WKPageGetGapBetweenPages(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->gapBetweenPages;
}

void APS5_VABI WKPageSetGapBetweenPages(const void* pageRef, double gap) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->gapBetweenPages = gap;
}

double APS5_VABI WKPageGetPageLength(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->pageLength;
}

void APS5_VABI WKPageSetPageLength(const void* pageRef, double length) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->pageLength = length;
}

}
