#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageGetMediaCaptureEnabled(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->mediaCaptureEnabled;
}

void APS5_VABI WKPageSetMediaCaptureEnabled(const void* pageRef, bool mediaCaptureEnabled) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->mediaCaptureEnabled = mediaCaptureEnabled;
}

}
