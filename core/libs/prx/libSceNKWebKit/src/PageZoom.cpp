#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cmath>

extern "C" {

double APS5_VABI WKPageGetPageZoomFactor(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->pageZoomFactor;
}

double APS5_VABI WKPageGetTextZoomFactor(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->textZoomFactor;
}

void APS5_VABI WKPageSetPageAndTextZoomFactors(const void* pageRef, double pageZoomFactor, double textZoomFactor) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!std::isfinite(pageZoomFactor) || pageZoomFactor <= 0.0 || !std::isfinite(textZoomFactor) || textZoomFactor <= 0.0) return;
    page->pageZoomFactor = pageZoomFactor;
    page->textZoomFactor = textZoomFactor;
}

void APS5_VABI WKPageSetPageZoomFactor(const void* pageRef, double zoomFactor) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!std::isfinite(zoomFactor) || zoomFactor <= 0.0) return;
    page->pageZoomFactor = zoomFactor;
}

void APS5_VABI WKPageSetTextZoomFactor(const void* pageRef, double zoomFactor) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!std::isfinite(zoomFactor) || zoomFactor <= 0.0) return;
    page->textZoomFactor = zoomFactor;
}

bool APS5_VABI WKPageSupportsTextZoom(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->supportsTextZoom;
}

}
