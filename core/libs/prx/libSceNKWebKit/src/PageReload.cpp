#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

namespace {

void ReloadPage(const void* pageRef, NKWebKit::ReloadMode mode, const char* function) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, function);
    if (page->isClosed) return;
    page->lastReloadMode = mode;
    ++page->reloadCount;
    if (page->committedURL) {
        page->provisionalURL = page->committedURL;
    } else if (page->activeURL) {
        page->provisionalURL = page->activeURL;
    }
}

}

extern "C" {

void APS5_VABI WKPageReload(const void* pageRef) {
    ReloadPage(pageRef, NKWebKit::ReloadMode::Normal, __func__);
}

void APS5_VABI WKPageReloadExpiredOnly(const void* pageRef) {
    ReloadPage(pageRef, NKWebKit::ReloadMode::ExpiredOnly, __func__);
}

void APS5_VABI WKPageReloadFromOrigin(const void* pageRef) {
    ReloadPage(pageRef, NKWebKit::ReloadMode::FromOrigin, __func__);
}

void APS5_VABI WKPageReloadWithoutContentBlockers(const void* pageRef) {
    ReloadPage(pageRef, NKWebKit::ReloadMode::WithoutContentBlockers, __func__);
}

void APS5_VABI WKPageStopLoading(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->provisionalURL = nullptr;
}

}
