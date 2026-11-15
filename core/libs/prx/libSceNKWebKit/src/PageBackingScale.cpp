#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

double APS5_VABI WKPageGetBackingScaleFactor(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->customBackingScaleFactor > 0.0) {
        return page->customBackingScaleFactor;
    }
    return 1.0;
}

void APS5_VABI WKPageSetCustomBackingScaleFactor(const void* pageRef, double customScaleFactor) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->customBackingScaleFactor = customScaleFactor;
}

}
