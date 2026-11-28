#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

NKWebKit::RectValue APS5_VABI WKBundleFrameGetContentBounds(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->contentBounds;
}

NKWebKit::RectValue APS5_VABI WKBundleFrameGetVisibleContentBounds(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->visibleContentBounds;
}

NKWebKit::RectValue APS5_VABI WKBundleFrameGetVisibleContentBoundsExcludingScrollbars(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->visibleContentBoundsExcludingScrollbars;
}

NKWebKit::SizeValue APS5_VABI WKBundleFrameGetScrollOffset(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->scrollOffset;
}

bool APS5_VABI WKBundleFrameHasHorizontalScrollbar(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->hasHorizontalScrollbar;
}

bool APS5_VABI WKBundleFrameHasVerticalScrollbar(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->hasVerticalScrollbar;
}

}
