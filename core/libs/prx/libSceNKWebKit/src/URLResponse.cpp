#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKURLResponseCopyURL(const void* responseRef) {
    const auto* response = NKWebKit::Cast<NKWebKit::URLResponse>(responseRef, __func__);
    if (!response->url) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(""));
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::URL>::Share(response->url.Get()));
}

const void* APS5_VABI WKURLResponseCopyMIMEType(const void* responseRef) {
    const auto* response = NKWebKit::Cast<NKWebKit::URLResponse>(responseRef, __func__);
    if (!response->mimeType) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(response->mimeType.Get()));
}

int APS5_VABI WKURLResponseHTTPStatusCode(const void* responseRef) {
    const auto* response = NKWebKit::Cast<NKWebKit::URLResponse>(responseRef, __func__);
    return response->statusCode;
}

const void* APS5_VABI WKURLResponseCopySuggestedFilename(const void* responseRef) {
    const auto* response = NKWebKit::Cast<NKWebKit::URLResponse>(responseRef, __func__);
    if (!response->suggestedFilename) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(response->suggestedFilename.Get()));
}

bool APS5_VABI WKURLResponseIsAttachment(const void* responseRef) {
    const auto* response = NKWebKit::Cast<NKWebKit::URLResponse>(responseRef, __func__);
    return response->isAttachment;
}

std::int64_t APS5_VABI WKURLResponseGetExpectedContentLength(const void* responseRef) {
    const auto* response = NKWebKit::Cast<NKWebKit::URLResponse>(responseRef, __func__);
    return response->expectedContentLength;
}

}
