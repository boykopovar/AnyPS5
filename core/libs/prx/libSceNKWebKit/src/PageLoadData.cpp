#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageLoadData(const void* pageRef, const void* dataRef, const void* MIMETypeRef, const void* encodingRef, const void* baseURLRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* data = NKWebKit::Cast<NKWebKit::Data>(dataRef, __func__);
    const auto* mimeType = NKWebKit::Cast<NKWebKit::String>(MIMETypeRef, __func__);
    const auto* encoding = NKWebKit::Cast<NKWebKit::String>(encodingRef, __func__);
    const auto* base = baseURLRef ? NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__) : nullptr;
    if (page->isClosed) return;
    page->loadedData = data->bytes;
    page->loadedMIMEType = mimeType->characters;
    page->loadedEncoding = encoding->characters;
    page->loadedBaseURL = base ? NKWebKit::Make<NKWebKit::URL>(base->text) : nullptr;
    page->provisionalURL = page->loadedBaseURL;
    page->activeURL = page->provisionalURL;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadDataWithUserData(const void* pageRef, const void* dataRef, const void* MIMETypeRef, const void* encodingRef, const void* baseURLRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* data = NKWebKit::Cast<NKWebKit::Data>(dataRef, __func__);
    const auto* mimeType = NKWebKit::Cast<NKWebKit::String>(MIMETypeRef, __func__);
    const auto* encoding = NKWebKit::Cast<NKWebKit::String>(encodingRef, __func__);
    const auto* base = baseURLRef ? NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__) : nullptr;
    if (page->isClosed) return;
    page->loadedData = data->bytes;
    page->loadedMIMEType = mimeType->characters;
    page->loadedEncoding = encoding->characters;
    page->loadedBaseURL = base ? NKWebKit::Make<NKWebKit::URL>(base->text) : nullptr;
    page->provisionalURL = page->loadedBaseURL;
    page->activeURL = page->provisionalURL;
    page->loadUserData = userDataRef;
}

}
