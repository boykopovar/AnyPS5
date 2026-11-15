#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageGetAllowsRemoteInspection(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->allowsRemoteInspection;
}

void APS5_VABI WKPageSetAllowsRemoteInspection(const void* pageRef, bool allowsRemoteInspection) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->allowsRemoteInspection = allowsRemoteInspection;
}

}
