#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKErrorCopyWKErrorDomain() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WebKitErrorDomain"));
}

const void* APS5_VABI WKErrorCopyDomain(const void* errorRef) {
    const auto* error = NKWebKit::Cast<NKWebKit::Error>(errorRef, __func__);
    if (!error->domain) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(error->domain.Get()));
}

int APS5_VABI WKErrorGetErrorCode(const void* errorRef) {
    const auto* error = NKWebKit::Cast<NKWebKit::Error>(errorRef, __func__);
    return error->errorCode;
}

const void* APS5_VABI WKErrorCopyFailingURL(const void* errorRef) {
    const auto* error = NKWebKit::Cast<NKWebKit::Error>(errorRef, __func__);
    if (!error->failingURL) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(std::string()));
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::URL>::Share(error->failingURL.Get()));
}

const void* APS5_VABI WKErrorCopyLocalizedDescription(const void* errorRef) {
    const auto* error = NKWebKit::Cast<NKWebKit::Error>(errorRef, __func__);
    if (!error->localizedDescription) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(error->localizedDescription.Get()));
}

}
