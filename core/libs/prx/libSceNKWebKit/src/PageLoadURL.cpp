#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageLoadURL(const void* pageRef, const void* urlRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    if (page->isClosed) {
        return;
    }
    auto target = NKWebKit::Make<NKWebKit::URL>(url->text);
    page->pendingAPIRequestURL = target;
    page->provisionalURL = target;
    page->activeURL = target;
    page->loadUserData = nullptr;
    page->shouldOpenExternalURLs = false;
}

void APS5_VABI WKPageLoadURLWithUserData(const void* pageRef, const void* urlRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    if (page->isClosed) {
        return;
    }
    auto target = NKWebKit::Make<NKWebKit::URL>(url->text);
    page->pendingAPIRequestURL = target;
    page->provisionalURL = target;
    page->activeURL = target;
    page->loadUserData = userDataRef;
    page->shouldOpenExternalURLs = false;
}

void APS5_VABI WKPageLoadURLWithShouldOpenExternalURLsPolicy(const void* pageRef, const void* urlRef, bool shouldOpenExternalURLs) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    if (page->isClosed) {
        return;
    }
    auto target = NKWebKit::Make<NKWebKit::URL>(url->text);
    page->pendingAPIRequestURL = target;
    page->provisionalURL = target;
    page->activeURL = target;
    page->loadUserData = nullptr;
    page->shouldOpenExternalURLs = shouldOpenExternalURLs;
}

void APS5_VABI WKPageLoadURLRequest(const void* pageRef, const void* urlRequestRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* req = NKWebKit::Cast<NKWebKit::URLRequest>(urlRequestRef, __func__);
    if (page->isClosed) {
        return;
    }
    page->activeURLRequest = NKWebKit::Retained<NKWebKit::URLRequest>::Share(const_cast<NKWebKit::URLRequest*>(req));
    if (req->url) {
        auto target = NKWebKit::Make<NKWebKit::URL>(req->url->text);
        page->pendingAPIRequestURL = target;
        page->provisionalURL = target;
        page->activeURL = target;
    }
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadURLRequestWithUserData(const void* pageRef, const void* urlRequestRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* req = NKWebKit::Cast<NKWebKit::URLRequest>(urlRequestRef, __func__);
    if (page->isClosed) {
        return;
    }
    page->activeURLRequest = NKWebKit::Retained<NKWebKit::URLRequest>::Share(const_cast<NKWebKit::URLRequest*>(req));
    if (req->url) {
        auto target = NKWebKit::Make<NKWebKit::URL>(req->url->text);
        page->pendingAPIRequestURL = target;
        page->provisionalURL = target;
        page->activeURL = target;
    }
    page->loadUserData = userDataRef;
}

void APS5_VABI WKPageLoadedSubresourceDomains(const void* pageRef, WKPageLoadedSubresourceDomainsFunction callback, void* context) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!callback) {
        throw std::invalid_argument("callback cannot be null");
    }
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(page->loadedSubresourceDomains.size());
    for (const auto& domain : page->loadedSubresourceDomains) {
        elements.push_back(NKWebKit::Make<NKWebKit::String>(NKWebKit::DecodeUtf8(domain.data(), domain.size()).value_or(std::u16string())));
    }
    auto array = NKWebKit::Make<NKWebKit::Array>(std::move(elements));
    callback(NKWebKit::ToRef(array), context);
}

}
