#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKAXObjectCopyTitle(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return object->title ? NKWebKit::ToRef(object->title) : nullptr;
}

const void* APS5_VABI WKAXObjectCopyDescription(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return object->description ? NKWebKit::ToRef(object->description) : nullptr;
}

const void* APS5_VABI WKAXObjectCopyHelpText(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return object->helpText ? NKWebKit::ToRef(object->helpText) : nullptr;
}

const void* APS5_VABI WKAXObjectCopyURL(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return object->url ? NKWebKit::ToRef(object->url) : nullptr;
}

}
