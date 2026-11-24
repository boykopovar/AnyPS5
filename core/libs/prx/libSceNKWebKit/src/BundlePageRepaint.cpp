#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKBundlePageCopyTrackedRepaintRects(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    return page->trackedRepaintRects ? NKWebKit::ToRef(page->trackedRepaintRects) : NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>());
}

bool APS5_VABI WKBundlePageIsTrackingRepaints(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->isTrackingRepaints;
}

void APS5_VABI WKBundlePageResetTrackedRepaints(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->trackedRepaintRects.Reset();
}

void APS5_VABI WKBundlePageSetTracksRepaints(const void* pageRef, bool tracksRepaints) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->isTrackingRepaints = tracksRepaints;
    if (!tracksRepaints) {
        page->trackedRepaintRects.Reset();
    }
}

}
