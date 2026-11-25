#include "prx/libSceNKWebKit/src/BackForwardList.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

bool APS5_VABI WKPageCanGoBack(const void* page) {
    const auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    return p->canGoBack;
}

bool APS5_VABI WKPageCanGoForward(const void* page) {
    const auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    return p->canGoForward;
}

void APS5_VABI WKPageGoBack(const void* page) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->canGoForward = true;
}

void APS5_VABI WKPageGoForward(const void* page) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->canGoBack = true;
}

void APS5_VABI WKPageGoToBackForwardListItem(const void* page, const void* item) {
    NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    NKWebKit::Cast<NKWebKit::BackForwardListItem>(item, __func__);
}

void APS5_VABI WKPageStopLoading(const void* page) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->estimatedProgress = 1.0;
}

void APS5_VABI WKPageTryClose(const void* page) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->isClosed = true;
}

}
