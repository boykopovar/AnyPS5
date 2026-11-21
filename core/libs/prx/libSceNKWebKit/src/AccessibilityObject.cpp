#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

namespace {

const void* CopiedString(const NKWebKit::Retained<NKWebKit::String>& value) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(value ? value->characters : std::u16string()));
}

}

extern "C" {

const void* APS5_VABI WKAXObjectCopyTitle(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return CopiedString(object->title);
}

const void* APS5_VABI WKAXObjectCopyDescription(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return CopiedString(object->description);
}

const void* APS5_VABI WKAXObjectCopyHelpText(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return CopiedString(object->helpText);
}

const void* APS5_VABI WKAXObjectCopyURL(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return CopiedString(object->url);
}

std::uint32_t APS5_VABI WKAXObjectButtonState(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return object->buttonState;
}

std::uint32_t APS5_VABI WKAXObjectRole(const void* objectRef) {
    const auto* object = NKWebKit::Cast<NKWebKit::AccessibilityObject>(objectRef, __func__);
    return object->role;
}

}
