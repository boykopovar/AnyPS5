#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageGetPaginationBehavesLikeColumns(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->paginationBehavesLikeColumns;
}

void APS5_VABI WKPageSetPaginationBehavesLikeColumns(const void* pageRef, bool value) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->paginationBehavesLikeColumns = value;
}

}
