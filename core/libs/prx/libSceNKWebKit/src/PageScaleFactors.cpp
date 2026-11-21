#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

double APS5_VABI WKPageGetBackingScaleFactor(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->customBackingScaleFactor.value_or(page->intrinsicBackingScaleFactor);
}

void APS5_VABI WKPageSetCustomBackingScaleFactor(const void* pageRef, double customScaleFactor) {
    auto* page = NKWebKit::MutablePage(pageRef, __func__);
    const auto factor = static_cast<float>(customScaleFactor);
    if (factor != 0.0f)
        page->customBackingScaleFactor = factor;
    else
        page->customBackingScaleFactor = std::nullopt;
}

double APS5_VABI WKPageGetScaleFactor(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->pageScaleFactor;
}

void APS5_VABI WKPageSetScaleFactor(const void* pageRef, double scale, NKWebKit::PointValue) {
    NKWebKit::MutablePage(pageRef, __func__)->pageScaleFactor = scale;
}

}
