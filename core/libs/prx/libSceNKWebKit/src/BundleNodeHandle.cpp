#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKBundleNodeHandleCopyDocument(const void* nodeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return handle->document ? NKWebKit::ToRef(handle->document) : nullptr;
}

const void* APS5_VABI WKBundleNodeHandleCopyDocumentFrame(const void* nodeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return handle->documentFrame ? NKWebKit::ToRef(handle->documentFrame) : nullptr;
}

const void* APS5_VABI WKBundleNodeHandleCopyHTMLIFrameElementContentFrame(const void* nodeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return handle->htmlIFrameElementContentFrame ? NKWebKit::ToRef(handle->htmlIFrameElementContentFrame) : nullptr;
}

void APS5_VABI WKBundleNodeHandleSetHTMLInputElementAutoFilled(const void* nodeHandleRef, bool autoFilled) {
    auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    handle->htmlInputElementAutoFilled = autoFilled;
}

bool APS5_VABI WKBundleNodeHandleGetHTMLInputElementAutoFilled(const void* nodeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return handle->htmlInputElementAutoFilled;
}

void APS5_VABI WKBundleNodeHandleSetHTMLInputElementAutofilled(const void* nodeHandleRef, bool autofilled) {
    auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    handle->htmlInputElementAutoFilled = autofilled;
}

bool APS5_VABI WKBundleNodeHandleGetHTMLInputElementAutofilled(const void* nodeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return handle->htmlInputElementAutoFilled;
}

void APS5_VABI WKBundleNodeHandleSetHTMLInputElementAutoFilledAndViewable(const void* nodeHandleRef, bool autoFilledAndViewable) {
    auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    handle->htmlInputElementAutoFilledAndViewable = autoFilledAndViewable;
}

void APS5_VABI WKBundleNodeHandleSetHTMLInputElementSpellcheckEnabled(const void* nodeHandleRef, bool enabled) {
    auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    handle->htmlInputElementSpellcheckEnabled = enabled;
}

void APS5_VABI WKBundleNodeHandleSetHTMLInputElementValueForUser(const void* nodeHandleRef, const void* valueRef) {
    auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    if (!valueRef) {
        handle->htmlInputElementValue = nullptr;
    } else {
        handle->htmlInputElementValue = NKWebKit::Retained<NKWebKit::String>::Share(
            NKWebKit::Cast<NKWebKit::String>(valueRef, __func__));
    }
    handle->htmlInputElementLastChangeWasUserEdit = true;
}

bool APS5_VABI WKBundleNodeHandleGetHTMLInputElementLastChangeWasUserEdit(const void* nodeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return handle->htmlInputElementLastChangeWasUserEdit;
}

bool APS5_VABI WKBundleNodeHandleGetHTMLTextAreaElementLastChangeWasUserEdit(const void* nodeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return handle->htmlTextAreaElementLastChangeWasUserEdit;
}

}
