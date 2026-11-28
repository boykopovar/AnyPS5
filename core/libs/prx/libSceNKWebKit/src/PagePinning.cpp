#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageIsPinnedToLeftSide(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->isPinnedToLeftSide;
}

bool APS5_VABI WKPageIsPinnedToRightSide(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->isPinnedToRightSide;
}

bool APS5_VABI WKPageIsPinnedToTopSide(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->isPinnedToTopSide;
}

bool APS5_VABI WKPageIsPinnedToBottomSide(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->isPinnedToBottomSide;
}

}
