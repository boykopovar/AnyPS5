#include "prx/libSceNKWebKit/src/FrameHandle.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKBundleFrameCreateFrameHandle(const void* bundleFrameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(bundleFrameRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::FrameHandle>(frame->frameID));
}

const void* APS5_VABI WKBundleFrameCopyCounterValue(const void* bundleFrameRef, const void* element) {
    NKWebKit::Cast<NKWebKit::BundleFrame>(bundleFrameRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
}

const void* APS5_VABI WKBundleFrameCopyInnerText(const void* bundleFrameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(bundleFrameRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->innerText));
}

const void* APS5_VABI WKBundleFrameCopyLayerTreeAsText(const void* bundleFrameRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(bundleFrameRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->layerTreeAsText));
}

const void* APS5_VABI WKBundleFrameCopyMIMETypeForResourceWithURL(const void* bundleFrameRef, const void* urlRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(bundleFrameRef, __func__);
    NKWebKit::CastOrNull<NKWebKit::URL>(urlRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->mimeType));
}

const void* APS5_VABI WKBundleFrameCopySuggestedFilenameForResourceWithURL(const void* bundleFrameRef, const void* urlRef) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(bundleFrameRef, __func__);
    NKWebKit::CastOrNull<NKWebKit::URL>(urlRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->suggestedFilename));
}

bool APS5_VABI WKBundleFrameGetDocumentBackgroundColor(
    const void* bundleFrameRef, double* red, double* green, double* blue, double* alpha) {
    const auto* frame = NKWebKit::Cast<NKWebKit::BundleFrame>(bundleFrameRef, __func__);
    if (!red || !green || !blue || !alpha) {
        return false;
    }
    if (frame->backgroundColor) {
        *red = frame->backgroundColor->red;
        *green = frame->backgroundColor->green;
        *blue = frame->backgroundColor->blue;
        *alpha = frame->backgroundColor->alpha;
        return true;
    }
    *red = 1.0;
    *green = 1.0;
    *blue = 1.0;
    *alpha = 1.0;
    return false;
}

}
