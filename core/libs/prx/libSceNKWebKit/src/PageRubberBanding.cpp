#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageHorizontalRubberBandingIsEnabled(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->horizontalRubberBanding;
}

bool APS5_VABI WKPageRubberBandsAtBottom(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->rubberBandsAtBottom;
}

bool APS5_VABI WKPageRubberBandsAtLeft(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->rubberBandsAtLeft;
}

bool APS5_VABI WKPageRubberBandsAtRight(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->rubberBandsAtRight;
}

bool APS5_VABI WKPageRubberBandsAtTop(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->rubberBandsAtTop;
}

void APS5_VABI WKPageSetEnableHorizontalRubberBanding(const void* pageRef, bool enable) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->horizontalRubberBanding = enable;
}

void APS5_VABI WKPageSetEnableVerticalRubberBanding(const void* pageRef, bool enable) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->verticalRubberBanding = enable;
}

void APS5_VABI WKPageSetRubberBandsAtBottom(const void* pageRef, bool enable) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->rubberBandsAtBottom = enable;
}

void APS5_VABI WKPageSetRubberBandsAtLeft(const void* pageRef, bool enable) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->rubberBandsAtLeft = enable;
}

void APS5_VABI WKPageSetRubberBandsAtRight(const void* pageRef, bool enable) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->rubberBandsAtRight = enable;
}

void APS5_VABI WKPageSetRubberBandsAtTop(const void* pageRef, bool enable) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->rubberBandsAtTop = enable;
}

bool APS5_VABI WKPageVerticalRubberBandingIsEnabled(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->verticalRubberBanding;
}

}
