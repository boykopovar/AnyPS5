#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageLoadHTMLString(const void* pageRef, const void* htmlStringRef, const void* baseURLRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* html = NKWebKit::Cast<NKWebKit::String>(htmlStringRef, __func__);
    const auto* base = baseURLRef ? NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__) : nullptr;
    if (page->isClosed) return;
    page->loadedHTML = html->characters;
    page->loadedPlainText.reset();
    page->loadedBaseURL = base ? NKWebKit::Make<NKWebKit::URL>(base->text) : nullptr;
    page->provisionalURL = page->loadedBaseURL;
    page->activeURL = page->provisionalURL;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadHTMLStringWithUserData(const void* pageRef, const void* htmlStringRef, const void* baseURLRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* html = NKWebKit::Cast<NKWebKit::String>(htmlStringRef, __func__);
    const auto* base = baseURLRef ? NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__) : nullptr;
    if (page->isClosed) return;
    page->loadedHTML = html->characters;
    page->loadedPlainText.reset();
    page->loadedBaseURL = base ? NKWebKit::Make<NKWebKit::URL>(base->text) : nullptr;
    page->provisionalURL = page->loadedBaseURL;
    page->activeURL = page->provisionalURL;
    page->loadUserData = userDataRef;
}

void APS5_VABI WKPageLoadPlainTextString(const void* pageRef, const void* plainTextStringRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* text = NKWebKit::Cast<NKWebKit::String>(plainTextStringRef, __func__);
    if (page->isClosed) return;
    page->loadedPlainText = text->characters;
    page->loadedHTML.reset();
    page->loadedBaseURL = nullptr;
    page->provisionalURL = nullptr;
    page->activeURL = nullptr;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadPlainTextStringWithUserData(const void* pageRef, const void* plainTextStringRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* text = NKWebKit::Cast<NKWebKit::String>(plainTextStringRef, __func__);
    if (page->isClosed) return;
    page->loadedPlainText = text->characters;
    page->loadedHTML.reset();
    page->loadedBaseURL = nullptr;
    page->provisionalURL = nullptr;
    page->activeURL = nullptr;
    page->loadUserData = userDataRef;
}

}
