#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageLoadWebArchiveData(const void* pageRef, const void* webArchiveDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* data = NKWebKit::Cast<NKWebKit::Data>(webArchiveDataRef, __func__);
    if (page->isClosed) return;
    page->loadedWebArchiveData = data->bytes;
    page->loadUserData = nullptr;
}

void APS5_VABI WKPageLoadWebArchiveDataWithUserData(const void* pageRef, const void* webArchiveDataRef, const void* userDataRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* data = NKWebKit::Cast<NKWebKit::Data>(webArchiveDataRef, __func__);
    if (page->isClosed) return;
    page->loadedWebArchiveData = data->bytes;
    page->loadUserData = userDataRef;
}

}
