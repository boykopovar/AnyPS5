#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

double APS5_VABI WKPageGetGapBetweenPages(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->gapBetweenPages;
}

void APS5_VABI WKPageSetGapBetweenPages(const void* pageRef, double value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->gapBetweenPages = value;
}

}
