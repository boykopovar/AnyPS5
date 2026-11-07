#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKDownloadCopyRequest(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->request ? NKWebKit::ToRef(download->request) : nullptr;
}

const void* APS5_VABI WKDownloadGetOriginatingPage(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->originatingPage ? download->originatingPage.Get() : nullptr;
}

bool APS5_VABI WKDownloadGetWasUserInitiated(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->wasUserInitiated;
}

}
