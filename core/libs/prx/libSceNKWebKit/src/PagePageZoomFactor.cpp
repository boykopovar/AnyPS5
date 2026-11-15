#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

double APS5_VABI WKPageGetPageZoomFactor(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->pageZoomFactor;
}

void APS5_VABI WKPageSetPageZoomFactor(const void* pageRef, double value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->pageZoomFactor = value;
}

}
