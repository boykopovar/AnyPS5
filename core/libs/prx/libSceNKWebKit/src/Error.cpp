#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKErrorCopyWKErrorDomain() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WebKitErrorDomain"));
}

const void* APS5_VABI WKErrorCopyDomain(const void* errorRef) {
    const auto* error = NKWebKit::Cast<NKWebKit::Error>(errorRef, __func__);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(error->domain.c_str()));
}

int APS5_VABI WKErrorGetErrorCode(const void* errorRef) {
    const auto* error = NKWebKit::Cast<NKWebKit::Error>(errorRef, __func__);
    return error->errorCode;
}

const void* APS5_VABI WKErrorCopyFailingURL(const void* errorRef) {
    const auto* error = NKWebKit::Cast<NKWebKit::Error>(errorRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(error->failingURL));
}

const void* APS5_VABI WKErrorCopyLocalizedDescription(const void* errorRef) {
    const auto* error = NKWebKit::Cast<NKWebKit::Error>(errorRef, __func__);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(error->localizedDescription.c_str()));
}

}
