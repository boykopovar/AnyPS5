#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundlePageSetBottomOverhangImage(const void* pageRef, const void* imageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->bottomOverhangImage = imageRef
        ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Image*>(NKWebKit::Cast<NKWebKit::Image>(imageRef, __func__)))
        : nullptr;
}

void APS5_VABI WKBundlePageSetTopOverhangImage(const void* pageRef, const void* imageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->topOverhangImage = imageRef
        ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Image*>(NKWebKit::Cast<NKWebKit::Image>(imageRef, __func__)))
        : nullptr;
}

}
