#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

bool APS5_VABI WKBundlePageHasLocalDataForURL(const void* pageRef, const void* urlRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return page->hasLocalDataForURL;
}

}
