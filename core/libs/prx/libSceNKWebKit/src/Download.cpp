#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKDownloadCancel(const void* downloadRef) {
    auto* download = const_cast<NKWebKit::Download*>(NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__));
    download->isCancelled = true;
}

const void* APS5_VABI WKDownloadCopyRedirectChain(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->redirectChain ? NKWebKit::ToRef(download->redirectChain) : NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>());
}

const void* APS5_VABI WKDownloadCopyRequest(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->request ? NKWebKit::ToRef(download->request) : nullptr;
}

std::uint64_t APS5_VABI WKDownloadGetID(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->id;
}

const void* APS5_VABI WKDownloadGetOriginatingPage(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->originatingPage ? download->originatingPage.Get() : nullptr;
}

const void* APS5_VABI WKDownloadGetResumeData(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->resumeData ? download->resumeData.Get() : nullptr;
}

bool APS5_VABI WKDownloadGetWasUserInitiated(const void* downloadRef) {
    const auto* download = NKWebKit::Cast<NKWebKit::Download>(downloadRef, __func__);
    return download->wasUserInitiated;
}

}
