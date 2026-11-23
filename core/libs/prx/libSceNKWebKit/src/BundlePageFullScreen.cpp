#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundlePageWillEnterFullScreen(const void* pageRef) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
}

void APS5_VABI WKBundlePageDidEnterFullScreen(const void* pageRef) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->isFullScreen = true;
}

void APS5_VABI WKBundlePageWillExitFullScreen(const void* pageRef) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
}

void APS5_VABI WKBundlePageDidExitFullScreen(const void* pageRef) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->isFullScreen = false;
}

}
