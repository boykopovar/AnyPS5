#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageSetMediaVolume(const void* page, float volume) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->mediaVolume = volume;
}

void APS5_VABI WKPageSetMuted(const void* page, bool muted) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->muted = muted;
}

bool APS5_VABI WKPageIsPlayingAudio(const void* page) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    return p->isPlayingAudio;
}

}
