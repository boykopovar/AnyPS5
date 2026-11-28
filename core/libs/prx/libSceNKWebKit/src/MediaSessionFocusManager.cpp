#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

const void* APS5_VABI WKContextGetMediaSessionFocusManager(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->mediaSessionFocusManager) {
        pool->mediaSessionFocusManager = NKWebKit::Make<NKWebKit::MediaSessionFocusManager>();
    }
    return pool->mediaSessionFocusManager.Get();
}

}
