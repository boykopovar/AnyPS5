#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageLoadAlternateHTMLString(const void* pageRef, const void* htmlStringRef, const void* baseURLRef, const void* unreachableURLRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* html = NKWebKit::Cast<NKWebKit::String>(htmlStringRef, __func__);
    const auto* base = baseURLRef ? NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__) : nullptr;
    const auto* unreachable = unreachableURLRef ? NKWebKit::Cast<NKWebKit::URL>(unreachableURLRef, __func__) : nullptr;
    if (page->isClosed) return;
    page->loadedAlternateHTML = html->characters;
    page->loadedBaseURL = base ? NKWebKit::Make<NKWebKit::URL>(base->text) : nullptr;
    page->unreachableURL = unreachable ? NKWebKit::Make<NKWebKit::URL>(unreachable->text) : nullptr;
    page->provisionalURL = page->unreachableURL ? page->unreachableURL : page->loadedBaseURL;
    page->activeURL = page->provisionalURL;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadAlternateHTMLStringWithUserData(const void* pageRef, const void* htmlStringRef, const void* baseURLRef, const void* unreachableURLRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* html = NKWebKit::Cast<NKWebKit::String>(htmlStringRef, __func__);
    const auto* base = baseURLRef ? NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__) : nullptr;
    const auto* unreachable = unreachableURLRef ? NKWebKit::Cast<NKWebKit::URL>(unreachableURLRef, __func__) : nullptr;
    if (page->isClosed) return;
    page->loadedAlternateHTML = html->characters;
    page->loadedBaseURL = base ? NKWebKit::Make<NKWebKit::URL>(base->text) : nullptr;
    page->unreachableURL = unreachable ? NKWebKit::Make<NKWebKit::URL>(unreachable->text) : nullptr;
    page->provisionalURL = page->unreachableURL ? page->unreachableURL : page->loadedBaseURL;
    page->activeURL = page->provisionalURL;
    page->loadUserData = userDataRef;
}

}
