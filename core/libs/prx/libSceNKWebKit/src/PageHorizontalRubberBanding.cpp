#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageHorizontalRubberBandingIsEnabled(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->horizontalRubberBandingEnabled;
}

void APS5_VABI WKPageSetEnableHorizontalRubberBanding(const void* pageRef, bool value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->horizontalRubberBandingEnabled = value;
}

}
