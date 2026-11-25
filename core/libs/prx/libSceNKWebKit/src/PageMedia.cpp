#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

std::uint32_t APS5_VABI WKPageGetMediaState(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->mediaState;
}

bool APS5_VABI WKPageHasMediaSessionWithActiveMediaElements(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->hasMediaSessionWithActiveMediaElements;
}

void APS5_VABI WKPageSetMayStartMediaWhenInWindow(const void* pageRef, bool mayStartMedia) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->mayStartMediaWhenInWindow = mayStartMedia;
}

void APS5_VABI WKPageSetMediaVolume(const void* pageRef, float volume) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->mediaVolume = volume;
}

void APS5_VABI WKPageSetMuted(const void* pageRef, std::uint32_t muted) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->mutedState = muted;
}

}
