#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKBundleFrameCopyCounterValue(const void* frameRef, const void* nodeHandleRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return frame->counterValue ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->counterValue->characters))
                               : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopyInnerText(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->innerText ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->innerText->characters))
                            : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopyLayerTreeAsText(const void* frameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->layerTreeAsText ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->layerTreeAsText->characters))
                                  : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopyMIMETypeForResourceWithURL(const void* frameRef, const void* urlRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return frame->mimeTypeForResource ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->mimeTypeForResource->characters))
                                      : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopySuggestedFilenameForResourceWithURL(const void* frameRef, const void* urlRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return frame->suggestedFilenameForResource
        ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->suggestedFilenameForResource->characters))
        : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

bool APS5_VABI WKBundleFrameGetDocumentBackgroundColor(const void* frameRef, double* red, double* green, double* blue, double* alpha) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(frameRef, __func__);
    if (!red || !green || !blue || !alpha) {
        throw std::invalid_argument(std::string(__func__) + ": null pointer argument");
    }
    if (frame->hasDocumentBackgroundColor) {
        *red = frame->documentBackgroundColor[0];
        *green = frame->documentBackgroundColor[1];
        *blue = frame->documentBackgroundColor[2];
        *alpha = frame->documentBackgroundColor[3];
        return true;
    }
    *red = 1.0;
    *green = 1.0;
    *blue = 1.0;
    *alpha = 1.0;
    return false;
}

}
