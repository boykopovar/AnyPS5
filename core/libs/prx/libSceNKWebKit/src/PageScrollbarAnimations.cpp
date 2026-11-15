#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageAreScrollbarAnimationsSuppressed(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->scrollbarAnimationsSuppressed;
}

void APS5_VABI WKPageSetSuppressScrollbarAnimations(const void* pageRef, bool value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->scrollbarAnimationsSuppressed = value;
}

}
