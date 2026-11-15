#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageRubberBandsAtLeft(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->rubberBandsAtLeft;
}

void APS5_VABI WKPageSetRubberBandsAtLeft(const void* pageRef, bool value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->rubberBandsAtLeft = value;
}

}
