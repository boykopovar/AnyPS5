#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKBundleFrameClearOpener(const void* frameRef) {
    auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    frame->hasOpener = false;
}

void APS5_VABI WKBundleFrameFocus(const void* frameRef) {
    auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    frame->isFocused = true;
}

}
