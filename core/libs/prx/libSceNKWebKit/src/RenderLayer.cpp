#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKRenderLayerCopyElementID(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->elementID ? NKWebKit::ToRef(layer->elementID) : nullptr;
}

const void* APS5_VABI WKRenderLayerCopyElementTagName(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->elementTagName ? NKWebKit::ToRef(layer->elementTagName) : nullptr;
}

const void* APS5_VABI WKRenderLayerCopyRendererName(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->rendererName ? NKWebKit::ToRef(layer->rendererName) : nullptr;
}

std::uint32_t APS5_VABI WKRenderLayerGetBackingStoreMemoryEstimate(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->backingStoreMemoryEstimate;
}

std::uint32_t APS5_VABI WKRenderLayerGetCompositingLayerType(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->compositingLayerType;
}

bool APS5_VABI WKRenderLayerIsClipped(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->isClipped;
}

bool APS5_VABI WKRenderLayerIsClipping(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->isClipping;
}

bool APS5_VABI WKRenderLayerIsReflection(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->isReflection;
}

}
