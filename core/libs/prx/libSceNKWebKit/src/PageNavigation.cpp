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
    if (p->isClosed) {
        return;
    }
    p->navigatedBack = true;
}

void APS5_VABI WKPageGoForward(const void* page) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    if (p->isClosed) {
        return;
    }
    p->navigatedForward = true;
}

void APS5_VABI WKPageGoToBackForwardListItem(const void* page, const void* itemRef) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    const auto* item = NKWebKit::Cast<NKWebKit::BackForwardListItem>(itemRef, __func__);
    if (p->isClosed) {
        return;
    }
    p->currentBackForwardListItem = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::BackForwardListItem*>(item));
}

}
