#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageLoadURL(const void* pageRef, const void* urlRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    if (page->isClosed) return;
    page->provisionalURL = NKWebKit::Make<NKWebKit::URL>(url->text);
    page->activeURL = page->provisionalURL;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadURLRequest(const void* pageRef, const void* urlRequestRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(urlRequestRef, __func__);
    if (page->isClosed) return;
    if (request->url) {
        page->provisionalURL = NKWebKit::Make<NKWebKit::URL>(request->url->text);
        page->activeURL = page->provisionalURL;
    }
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadURLRequestWithUserData(const void* pageRef, const void* urlRequestRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(urlRequestRef, __func__);
    if (page->isClosed) return;
    if (request->url) {
        page->provisionalURL = NKWebKit::Make<NKWebKit::URL>(request->url->text);
        page->activeURL = page->provisionalURL;
    }
    page->loadUserData = userDataRef;
}

void APS5_VABI WKPageLoadURLWithShouldOpenExternalURLsPolicy(const void* pageRef, const void* urlRef, bool shouldOpenExternalURLs) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    if (page->isClosed) return;
    page->provisionalURL = NKWebKit::Make<NKWebKit::URL>(url->text);
    page->activeURL = page->provisionalURL;
    page->shouldOpenExternalURLsPolicy = shouldOpenExternalURLs;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadURLWithUserData(const void* pageRef, const void* urlRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    if (page->isClosed) return;
    page->provisionalURL = NKWebKit::Make<NKWebKit::URL>(url->text);
    page->activeURL = page->provisionalURL;
    page->loadUserData = userDataRef;
}

}
