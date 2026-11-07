#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

namespace {

double s_defaultTimeoutInterval = 0.0;

}

extern "C" {

void APS5_VABI WKURLRequestSetDefaultTimeoutInterval(double timeoutInterval) {
    s_defaultTimeoutInterval = timeoutInterval;
}

const void* APS5_VABI WKURLRequestCreateWithWKURL(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    (void)url;
    auto req = NKWebKit::Make<NKWebKit::URLRequest>();
    req->url = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(urlRef, __func__)));
    req->timeoutInterval = s_defaultTimeoutInterval;
    req->httpMethod = "GET";
    return NKWebKit::ToRef(req);
}

const void* APS5_VABI WKURLRequestCopyURL(const void* requestRef) {
    const auto* req = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    return req->url ? NKWebKit::ToRef(req->url) : nullptr;
}

const void* APS5_VABI WKURLRequestCopyFirstPartyForCookies(const void* requestRef) {
    const auto* req = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    return req->firstPartyForCookies ? NKWebKit::ToRef(req->firstPartyForCookies) : nullptr;
}

const void* APS5_VABI WKURLRequestCopyHTTPMethod(const void* requestRef) {
    const auto* req = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(req->httpMethod.c_str()));
}

const void* APS5_VABI WKURLRequestCopySettingHTTPBody(const void* requestRef, const void* bodyDataRef) {
    const auto* req = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    if (bodyDataRef) {
        NKWebKit::Cast<NKWebKit::Data>(bodyDataRef, __func__);
    }
    auto copy = NKWebKit::Make<NKWebKit::URLRequest>();
    copy->url = req->url;
    copy->firstPartyForCookies = req->firstPartyForCookies;
    copy->httpMethod = req->httpMethod;
    copy->timeoutInterval = req->timeoutInterval;
    if (bodyDataRef) {
        copy->httpBody = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(bodyDataRef, __func__)));
    }
    return NKWebKit::ToRef(copy);
}

}
