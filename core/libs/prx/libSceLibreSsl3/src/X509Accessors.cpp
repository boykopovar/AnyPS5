#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {

const Asn1String* APS5_VABI X509_get0_notBefore_nid_postfix(const LibreSsl3X509* certificate) {
    if (!certificate) throw std::invalid_argument("X509_get0_notBefore requires a certificate");
    if (!certificate->certInfo) throw std::invalid_argument("X509_get0_notBefore requires cert info");
    if (!certificate->certInfo->validity) return nullptr;
    return certificate->certInfo->validity->notBefore;
}

const Asn1String* APS5_VABI X509_get0_notAfter_nid_postfix(const LibreSsl3X509* certificate) {
    if (!certificate) throw std::invalid_argument("X509_get0_notAfter requires a certificate");
    if (!certificate->certInfo) throw std::invalid_argument("X509_get0_notAfter requires cert info");
    if (!certificate->certInfo->validity) return nullptr;
    return certificate->certInfo->validity->notAfter;
}

Asn1String* APS5_VABI X509_getm_notBefore_nid_postfix(LibreSsl3X509* certificate) {
    if (!certificate) throw std::invalid_argument("X509_getm_notBefore requires a certificate");
    if (!certificate->certInfo) throw std::invalid_argument("X509_getm_notBefore requires cert info");
    if (!certificate->certInfo->validity) return nullptr;
    return certificate->certInfo->validity->notBefore;
}

Asn1String* APS5_VABI X509_getm_notAfter_nid_postfix(LibreSsl3X509* certificate) {
    if (!certificate) throw std::invalid_argument("X509_getm_notAfter requires a certificate");
    if (!certificate->certInfo) throw std::invalid_argument("X509_getm_notAfter requires cert info");
    if (!certificate->certInfo->validity) return nullptr;
    return certificate->certInfo->validity->notAfter;
}

void APS5_VABI X509_get0_signature_nid_postfix(const Asn1String** signature, void** algorithm, const LibreSsl3X509* certificate) {
    if (!certificate) throw std::invalid_argument("X509_get0_signature requires a certificate");
    if (signature) *signature = static_cast<const Asn1String*>(certificate->signature);
    if (algorithm) *algorithm = certificate->sigAlg;
}

void* APS5_VABI X509_get_ext_nid_postfix(const LibreSsl3X509* certificate, int location) {
    if (!certificate) throw std::invalid_argument("X509_get_ext requires a certificate");
    if (!certificate->certInfo || !certificate->certInfo->extensions) return nullptr;
    auto* const stack = certificate->certInfo->extensions;
    if (location < 0 || location >= stack->num) return nullptr;
    return stack->data[location];
}

int APS5_VABI X509_get_ext_count_nid_postfix(const LibreSsl3X509* certificate) {
    if (!certificate) throw std::invalid_argument("X509_get_ext_count requires a certificate");
    if (!certificate->certInfo || !certificate->certInfo->extensions) return 0;
    return certificate->certInfo->extensions->num;
}

void* APS5_VABI X509_get0_extensions_nid_postfix(const LibreSsl3X509* certificate) {
    if (!certificate) throw std::invalid_argument("X509_get0_extensions requires a certificate");
    if (!certificate->certInfo) return nullptr;
    return certificate->certInfo->extensions;
}

}
