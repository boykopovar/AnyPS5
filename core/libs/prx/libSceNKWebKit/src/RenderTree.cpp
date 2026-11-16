#include "prx/libSceNKWebKit/src/RenderTree.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKRenderLayerCopyElementID(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->elementID ? NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(layer->elementID.Get())) : nullptr;
}

const void* APS5_VABI WKRenderLayerCopyElementTagName(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->elementTagName ? NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(layer->elementTagName.Get())) : nullptr;
}

const void* APS5_VABI WKRenderLayerCopyRendererName(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->rendererName ? NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(layer->rendererName.Get())) : nullptr;
}

NKWebKit::RectValue APS5_VABI WKRenderLayerGetAbsoluteBounds(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->absoluteBounds;
}

std::uint32_t APS5_VABI WKRenderLayerGetBackingStoreMemoryEstimate(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->backingStoreMemoryEstimate;
}

std::uint32_t APS5_VABI WKRenderLayerGetCompositingLayerType(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->compositingLayerType;
}

const void* APS5_VABI WKRenderLayerGetElementClassNames(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->elementClassNames ? layer->elementClassNames.Get() : nullptr;
}

const void* APS5_VABI WKRenderLayerGetFrameContentsLayer(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->frameContentsLayer ? layer->frameContentsLayer.Get() : nullptr;
}

const void* APS5_VABI WKRenderLayerGetNegativeZOrderList(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->negativeZOrderList ? layer->negativeZOrderList.Get() : nullptr;
}

const void* APS5_VABI WKRenderLayerGetNormalFlowList(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->normalFlowList ? layer->normalFlowList.Get() : nullptr;
}

const void* APS5_VABI WKRenderLayerGetPositiveZOrderList(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->positiveZOrderList ? layer->positiveZOrderList.Get() : nullptr;
}

const void* APS5_VABI WKRenderLayerGetRenderer(const void* layerRef) {
    const auto* layer = NKWebKit::Cast<NKWebKit::RenderLayer>(layerRef, __func__);
    return layer->renderer ? layer->renderer.Get() : nullptr;
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

const void* APS5_VABI WKRenderObjectCopyElementID(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->elementID ? NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(obj->elementID.Get())) : nullptr;
}

const void* APS5_VABI WKRenderObjectCopyElementTagName(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->elementTagName ? NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(obj->elementTagName.Get())) : nullptr;
}

const void* APS5_VABI WKRenderObjectCopyName(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->name ? NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(obj->name.Get())) : nullptr;
}

const void* APS5_VABI WKRenderObjectCopyTextSnippet(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->textSnippet ? NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(obj->textSnippet.Get())) : nullptr;
}

NKWebKit::PointValue APS5_VABI WKRenderObjectGetAbsolutePosition(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->absolutePosition;
}

const void* APS5_VABI WKRenderObjectGetChildren(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->children ? obj->children.Get() : nullptr;
}

const void* APS5_VABI WKRenderObjectGetElementClassNames(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->elementClassNames ? obj->elementClassNames.Get() : nullptr;
}

NKWebKit::RectValue APS5_VABI WKRenderObjectGetFrameRect(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->frameRect;
}

std::uint32_t APS5_VABI WKRenderObjectGetTextLength(const void* objectRef) {
    const auto* obj = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return obj->textLength;
}

}
