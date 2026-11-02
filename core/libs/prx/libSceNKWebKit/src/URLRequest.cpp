#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKURLRequestCreateWithWKURL(const void* urlRef) {
    if (!urlRef) return nullptr;
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URLRequest>(NKWebKit::Make<NKWebKit::URL>(url->text)));
}

const void* APS5_VABI WKURLRequestCopyURL(const void* requestRef) {
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    if (!request->url) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(request->url->text));
}

const void* APS5_VABI WKURLRequestCopyFirstPartyForCookies(const void* requestRef) {
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    if (!request->firstPartyForCookies) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(request->firstPartyForCookies->text));
}

const void* APS5_VABI WKURLRequestCopyHTTPMethod(const void* requestRef) {
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(request->httpMethod.c_str()));
}

const void* APS5_VABI WKURLRequestCopySettingHTTPBody(const void* requestRef, const void* bodyRef) {
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    const auto* body = NKWebKit::CastOrNull<NKWebKit::Data>(bodyRef, __func__);
    auto copy = NKWebKit::Make<NKWebKit::URLRequest>(request->url ? NKWebKit::Make<NKWebKit::URL>(request->url->text) : nullptr);
    if (request->firstPartyForCookies) {
        copy->firstPartyForCookies = NKWebKit::Make<NKWebKit::URL>(request->firstPartyForCookies->text);
    }
    copy->httpMethod = request->httpMethod;
    if (body) {
        copy->httpBody = NKWebKit::Make<NKWebKit::Data>(body->bytes);
    }
    return NKWebKit::ToRef(std::move(copy));
}

void APS5_VABI WKURLRequestSetDefaultTimeoutInterval(double timeoutInterval) {
    NKWebKit::URLRequest::defaultTimeoutInterval = timeoutInterval;
}

}
