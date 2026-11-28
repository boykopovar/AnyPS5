#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageClearLoadedSubresourceDomains(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->loadedSubresourceDomains.clear();
}

void APS5_VABI WKPageClearUserMediaState(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->userMediaStateCleared = true;
}

void APS5_VABI WKPageClearWheelEventTestMonitor(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->wheelEventTestMonitorCleared = true;
}

}
