#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

std::uint32_t APS5_VABI WKPageGetPaginationMode(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->paginationMode;
}

void APS5_VABI WKPageSetPaginationMode(const void* pageRef, std::uint32_t value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->paginationMode = value;
}

}
