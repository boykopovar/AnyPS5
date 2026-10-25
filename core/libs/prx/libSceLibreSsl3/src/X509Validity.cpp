#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSsl3X509Validity) == 16);
static_assert(sizeof(LibreSsl3ValidityCertInfo) == 104);
static_assert(offsetof(LibreSsl3ValidityCertInfo, validity) == 32);
static_assert(sizeof(LibreSsl3ValidityX509) == 216);
static_assert(offsetof(LibreSsl3ValidityX509, sigAlg) == 8);
static_assert(offsetof(LibreSsl3ValidityX509, signature) == 16);

namespace {

const LibreSsl3X509Validity* Validity(const LibreSsl3ValidityX509* certificate) {
    if (!certificate || !certificate->certInfo) return nullptr;
    return certificate->certInfo->validity;
}

}

extern "C" {

void* APS5_VABI X509_getm_notBefore_nid_postfix(const LibreSsl3ValidityX509* certificate) {
    const LibreSsl3X509Validity* validity = Validity(certificate);
    return validity ? validity->notBefore : nullptr;
}

void* APS5_VABI X509_getm_notAfter_nid_postfix(const LibreSsl3ValidityX509* certificate) {
    const LibreSsl3X509Validity* validity = Validity(certificate);
    return validity ? validity->notAfter : nullptr;
}

const void* APS5_VABI X509_get0_notBefore_nid_postfix(const LibreSsl3ValidityX509* certificate) {
    return X509_getm_notBefore_nid_postfix(certificate);
}

const void* APS5_VABI X509_get0_notAfter_nid_postfix(const LibreSsl3ValidityX509* certificate) {
    return X509_getm_notAfter_nid_postfix(certificate);
}

void APS5_VABI X509_get0_signature_nid_postfix(const void** signature, const void** algorithm, const LibreSsl3ValidityX509* certificate) {
    if (!certificate) throw std::invalid_argument(std::string(__func__) + ": null certificate");
    if (signature) *signature = certificate->signature;
    if (algorithm) *algorithm = certificate->sigAlg;
}

}
