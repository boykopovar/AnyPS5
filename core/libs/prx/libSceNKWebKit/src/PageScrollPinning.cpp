#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

std::uint32_t APS5_VABI WKPageGetScrollPinningBehavior(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->scrollPinningBehavior;
}

void APS5_VABI WKPageSetScrollPinningBehavior(const void* pageRef, std::uint32_t value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->scrollPinningBehavior = value;
}

}
