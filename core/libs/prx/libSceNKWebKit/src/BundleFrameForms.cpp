#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

bool APS5_VABI WKBundleFrameContainsAnyFormControls(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->containsAnyFormControls;
}

bool APS5_VABI WKBundleFrameContainsAnyFormElements(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->containsAnyFormElements;
}

}
