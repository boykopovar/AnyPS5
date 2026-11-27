#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageLoadHTMLString(const void* pageRef, const void* htmlStringRef, const void* baseURLRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* html = NKWebKit::Cast<NKWebKit::String>(htmlStringRef, __func__);
    const auto* base = baseURLRef ? NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__) : nullptr;
    if (page->isClosed) {
        return;
    }
    page->loadedHTMLString = html->characters;
    auto target = (base && !base->text.empty()) ? NKWebKit::Make<NKWebKit::URL>(base->text) : NKWebKit::Make<NKWebKit::URL>("about:blank");
    page->loadedBaseURL = target;
    page->pendingAPIRequestURL = target;
    page->provisionalURL = target;
    page->activeURL = target;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadHTMLStringWithUserData(const void* pageRef, const void* htmlStringRef, const void* baseURLRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* html = NKWebKit::Cast<NKWebKit::String>(htmlStringRef, __func__);
    const auto* base = baseURLRef ? NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__) : nullptr;
    if (page->isClosed) {
        return;
    }
    page->loadedHTMLString = html->characters;
    auto target = (base && !base->text.empty()) ? NKWebKit::Make<NKWebKit::URL>(base->text) : NKWebKit::Make<NKWebKit::URL>("about:blank");
    page->loadedBaseURL = target;
    page->pendingAPIRequestURL = target;
    page->provisionalURL = target;
    page->activeURL = target;
    page->loadUserData = userDataRef;
}

void APS5_VABI WKPageLoadPlainTextString(const void* pageRef, const void* plainTextStringRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* text = NKWebKit::Cast<NKWebKit::String>(plainTextStringRef, __func__);
    if (page->isClosed) {
        return;
    }
    page->loadedPlainTextString = text->characters;
    auto target = NKWebKit::Make<NKWebKit::URL>("about:blank");
    page->loadedBaseURL = target;
    page->pendingAPIRequestURL = target;
    page->provisionalURL = target;
    page->activeURL = target;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadPlainTextStringWithUserData(const void* pageRef, const void* plainTextStringRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* text = NKWebKit::Cast<NKWebKit::String>(plainTextStringRef, __func__);
    if (page->isClosed) {
        return;
    }
    page->loadedPlainTextString = text->characters;
    auto target = NKWebKit::Make<NKWebKit::URL>("about:blank");
    page->loadedBaseURL = target;
    page->pendingAPIRequestURL = target;
    page->provisionalURL = target;
    page->activeURL = target;
    page->loadUserData = userDataRef;
}

}
