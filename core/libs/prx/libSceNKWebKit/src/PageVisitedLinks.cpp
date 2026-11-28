#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageGetAddsVisitedLinks(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->addsVisitedLinks;
}

void APS5_VABI WKPageSetAddsVisitedLinks(const void* pageRef, bool addsVisitedLinks) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->addsVisitedLinks = addsVisitedLinks;
}

}
