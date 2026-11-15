#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageRubberBandsAtTop(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->rubberBandsAtTop;
}

void APS5_VABI WKPageSetRubberBandsAtTop(const void* pageRef, bool value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->rubberBandsAtTop = value;
}

}
