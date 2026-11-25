#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

bool APS5_VABI WKBundleFrameAllowsFollowingLink(const void* frameRef, const void* urlRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return frame->allowsFollowingLink;
}

bool APS5_VABI WKBundleFrameCallShouldCloseOnWebView(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->shouldCloseOnWebView;
}

void APS5_VABI WKBundleFrameSetAccessibleName(const void* frameRef, const void* accessibleNameRef) {
    auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    const auto* name = NKWebKit::Cast<NKWebKit::String>(accessibleNameRef, __func__);
    frame->accessibleName = name->characters;
}

void APS5_VABI WKBundleFrameSetTextDirection(const void* frameRef, const void* directionRef) {
    auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    const auto* dir = NKWebKit::Cast<NKWebKit::String>(directionRef, __func__);
    frame->textDirection = dir->characters;
}

}
