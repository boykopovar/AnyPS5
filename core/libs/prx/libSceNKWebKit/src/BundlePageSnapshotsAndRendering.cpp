#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <algorithm>
#include <cmath>

extern "C" {

const void* APS5_VABI WKBundlePageCreateSnapshotInViewCoordinates(const void* pageRef, NKWebKit::RectValue rect, std::uint32_t options) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    int width = static_cast<int>(std::max(0.0, std::ceil(rect.size.width)));
    int height = static_cast<int>(std::max(0.0, std::ceil(rect.size.height)));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Image>(width, height));
}

const void* APS5_VABI WKBundlePageCreateSnapshotInDocumentCoordinates(const void* pageRef, NKWebKit::RectValue rect, std::uint32_t options) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    int width = static_cast<int>(std::max(0.0, std::ceil(rect.size.width)));
    int height = static_cast<int>(std::max(0.0, std::ceil(rect.size.height)));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Image>(width, height));
}

const void* APS5_VABI WKBundlePageCreateScaledSnapshotInDocumentCoordinates(const void* pageRef, NKWebKit::RectValue rect, double scaleFactor, std::uint32_t options) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    double scale = scaleFactor > 0.0 ? scaleFactor : 1.0;
    int width = static_cast<int>(std::max(0.0, std::ceil(rect.size.width * scale)));
    int height = static_cast<int>(std::max(0.0, std::ceil(rect.size.height * scale)));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Image>(width, height));
}

const void* APS5_VABI WKBundlePageCreateSnapshotWithOptions(const void* pageRef, NKWebKit::RectValue rect, std::uint32_t options) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    int width = static_cast<int>(std::max(0.0, std::ceil(rect.size.width)));
    int height = static_cast<int>(std::max(0.0, std::ceil(rect.size.height)));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Image>(width, height));
}

const void* APS5_VABI WKBundlePageCopyRenderTreeExternalRepresentation(const void* pageRef, std::uint32_t behavior) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::DecodeUtf8(page->renderTreeExternalRepresentation.c_str())));
}

const void* APS5_VABI WKBundlePageCopyTrackedRepaintRects(const void* pageRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    return NKWebKit::ToRef(page->trackedRepaintRects ? page->trackedRepaintRects : NKWebKit::Make<NKWebKit::Array>());
}

void APS5_VABI WKBundlePageForceRepaint(const void* pageRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->forceRepaintCount++;
}

std::uint32_t APS5_VABI WKBundlePageExtendIncrementalRenderingSuppression(const void* pageRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    return ++page->nextRenderingSuppressionToken;
}

void APS5_VABI WKBundlePageStopExtendingIncrementalRenderingSuppression(const void* pageRef, std::uint32_t token) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->lastStoppedRenderingSuppressionToken = token;
}

}
