#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageClose(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->isClosed = true;
}

void APS5_VABI WKPageSuspendActiveDOMObjectsAndAnimations(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->activeDOMObjectsAndAnimationsSuspended = true;
}

void APS5_VABI WKPageResumeActiveDOMObjectsAndAnimations(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->activeDOMObjectsAndAnimationsSuspended = false;
}

}
