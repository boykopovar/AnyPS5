#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

double APS5_VABI WKPageGetPageLength(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->pageLength;
}

void APS5_VABI WKPageSetPageLength(const void* pageRef, double value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->pageLength = value;
}

}
