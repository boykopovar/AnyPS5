#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageBackgroundExtendsBeyondPage(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->backgroundExtendsBeyondPage;
}

void APS5_VABI WKPageSetBackgroundExtendsBeyondPage(const void* pageRef, bool backgroundExtendsBeyondPage) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->backgroundExtendsBeyondPage = backgroundExtendsBeyondPage;
}

}
