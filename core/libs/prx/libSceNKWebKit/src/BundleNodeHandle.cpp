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

}
