#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

bool APS5_VABI WKBundleFrameIsMainFrame(const void* frameRef) {
    return NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__)->isMainFrame;
}

const void* APS5_VABI WKBundleFrameGetParentFrame(const void* frameRef) {
    return NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__)->parentFrame.Get();
}

const void* APS5_VABI WKBundleFrameCopyChildFrames(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(frame->childFrames ? frame->childFrames->elements : std::vector<NKWebKit::Retained<NKWebKit::Object>>{}));
}

const void* APS5_VABI WKBundleFrameCopyName(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return NKWebKit::ToRef(frame->name ? frame->name : NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopyURL(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->url ? NKWebKit::ToRef(frame->url) : nullptr;
}

const void* APS5_VABI WKBundleFrameCopyProvisionalURL(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->provisionalURL ? NKWebKit::ToRef(frame->provisionalURL) : nullptr;
}

const void* APS5_VABI WKBundleFrameCopySecurityOrigin(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->securityOrigin ? NKWebKit::ToRef(frame->securityOrigin) : nullptr;
}

std::uint32_t APS5_VABI WKBundleFrameGetFrameLoadState(const void* frameRef) {
    return NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__)->loadState;
}

}
