#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

int APS5_VABI WKBundleNumberOfPages(const void* bundleRef, const void* frameRef, double pageWidthInPixels, double pageHeightInPixels) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    if (pageWidthInPixels <= 0.0 || pageHeightInPixels <= 0.0) {
        return 0;
    }
    return frame->numberOfPages;
}

bool APS5_VABI WKBundleIsPageBoxVisible(const void* bundleRef, const void* frameRef, int pageIndex) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    if (pageIndex < 0 || static_cast<std::size_t>(pageIndex) >= frame->pageBoxVisibility.size()) {
        return false;
    }
    return frame->pageBoxVisibility[static_cast<std::size_t>(pageIndex)];
}

}
