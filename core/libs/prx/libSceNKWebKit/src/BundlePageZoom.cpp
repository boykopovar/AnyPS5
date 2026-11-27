#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

double APS5_VABI WKBundlePageGetPageZoomFactor(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->pageZoomFactor;
}

double APS5_VABI WKBundlePageGetTextZoomFactor(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->textZoomFactor;
}

void APS5_VABI WKBundlePageSetPageZoomFactor(const void* pageRef, double zoomFactor) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    if (zoomFactor <= 0) return;
    page->pageZoomFactor = zoomFactor;
}

void APS5_VABI WKBundlePageSetTextZoomFactor(const void* pageRef, double zoomFactor) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    if (zoomFactor <= 0) return;
    page->textZoomFactor = zoomFactor;
}

}
