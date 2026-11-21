#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageLoadFile(const void* pageRef, const void* fileURLRef, const void* allowedDirectoryURLRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* fileURL = NKWebKit::Cast<NKWebKit::URL>(fileURLRef, __func__);
    const auto* allowedDir = allowedDirectoryURLRef ? NKWebKit::Cast<NKWebKit::URL>(allowedDirectoryURLRef, __func__) : nullptr;
    if (page->isClosed) return;
    page->loadedFileURL = NKWebKit::Make<NKWebKit::URL>(fileURL->text);
    page->allowedDirectoryURL = allowedDir ? NKWebKit::Make<NKWebKit::URL>(allowedDir->text) : nullptr;
    page->provisionalURL = page->loadedFileURL;
    page->activeURL = page->provisionalURL;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadFileWithUserData(const void* pageRef, const void* fileURLRef, const void* allowedDirectoryURLRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* fileURL = NKWebKit::Cast<NKWebKit::URL>(fileURLRef, __func__);
    const auto* allowedDir = allowedDirectoryURLRef ? NKWebKit::Cast<NKWebKit::URL>(allowedDirectoryURLRef, __func__) : nullptr;
    if (page->isClosed) return;
    page->loadedFileURL = NKWebKit::Make<NKWebKit::URL>(fileURL->text);
    page->allowedDirectoryURL = allowedDir ? NKWebKit::Make<NKWebKit::URL>(allowedDir->text) : nullptr;
    page->provisionalURL = page->loadedFileURL;
    page->activeURL = page->provisionalURL;
    page->loadUserData = userDataRef;
}

}
