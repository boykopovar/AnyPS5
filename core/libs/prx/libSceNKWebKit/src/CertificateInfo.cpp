#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKCertificateInfoCreateWithCertficateChain(const void* certificateChainRef) {
    return nullptr;
}

int APS5_VABI WKCertificateInfoGetVerificationError(const void* certificateInfoRef) {
    return 0;
}

const void* APS5_VABI WKCertificateInfoCopyVerificationErrorDescription(const void* certificateInfoRef) {
    return nullptr;
}

std::size_t APS5_VABI WKCertificateInfoGetCertificateChainSize(const void* certificateInfoRef) {
    return 0;
}

const void* APS5_VABI WKCertificateInfoCopyCertificateAtIndex(const void* certificateInfoRef, std::size_t index) {
    return nullptr;
}

}
