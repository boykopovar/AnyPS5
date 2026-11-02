#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKRenderObjectCopyElementID(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return object->elementID ? NKWebKit::ToRef(object->elementID) : nullptr;
}

const void* APS5_VABI WKRenderObjectCopyElementTagName(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return object->elementTagName ? NKWebKit::ToRef(object->elementTagName) : nullptr;
}

const void* APS5_VABI WKRenderObjectCopyName(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return object->name ? NKWebKit::ToRef(object->name) : nullptr;
}

const void* APS5_VABI WKRenderObjectCopyTextSnippet(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return object->textSnippet ? NKWebKit::ToRef(object->textSnippet) : nullptr;
}

std::uint32_t APS5_VABI WKRenderObjectGetTextLength(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::RenderObject>(objectRef, __func__);
    return object->textLength;
}

}
