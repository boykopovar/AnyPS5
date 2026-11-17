#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <cerrno>
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

bool CopyGuestInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

bool CopyGuestOutPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

const void* ReadCertificatePointer(const LibreSsl3ValidityX509* certificate, const void* const LibreSsl3ValidityX509::*field, const char* function) {
    const void* value = nullptr;
    if (!CopyGuestInPreservingErrno(&value, &(certificate->*field), sizeof(value))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

bool ReadValidity(const LibreSsl3ValidityX509* certificate, LibreSsl3X509Validity& outValidity, const char* caller) {
    if (!certificate) return false;
    const LibreSsl3ValidityCertInfo* certInfo = nullptr;
    if (!CopyGuestInPreservingErrno(&certInfo, &certificate->certInfo, sizeof(certInfo))) NotImplemented_nid_no_patch(caller);
    if (!certInfo) return false;
    const LibreSsl3X509Validity* validity = nullptr;
    if (!CopyGuestInPreservingErrno(&validity, &certInfo->validity, sizeof(validity))) NotImplemented_nid_no_patch(caller);
    if (!validity) return false;
    if (!CopyGuestInPreservingErrno(&outValidity, validity, sizeof(outValidity))) NotImplemented_nid_no_patch(caller);
    return true;
}

}

extern "C" {

void* APS5_VABI X509_getm_notBefore_nid_postfix(const LibreSsl3ValidityX509* certificate) {
    LibreSsl3X509Validity validity{};
    return ReadValidity(certificate, validity, __func__) ? validity.notBefore : nullptr;
}

void* APS5_VABI X509_getm_notAfter_nid_postfix(const LibreSsl3ValidityX509* certificate) {
    LibreSsl3X509Validity validity{};
    return ReadValidity(certificate, validity, __func__) ? validity.notAfter : nullptr;
}

const void* APS5_VABI X509_get0_notBefore_nid_postfix(const LibreSsl3ValidityX509* certificate) {
    LibreSsl3X509Validity validity{};
    return ReadValidity(certificate, validity, __func__) ? validity.notBefore : nullptr;
}

const void* APS5_VABI X509_get0_notAfter_nid_postfix(const LibreSsl3ValidityX509* certificate) {
    LibreSsl3X509Validity validity{};
    return ReadValidity(certificate, validity, __func__) ? validity.notAfter : nullptr;
}

void APS5_VABI X509_get0_signature_nid_postfix(const void** signature, const void** algorithm, const LibreSsl3ValidityX509* certificate) {
    if (!certificate && (signature || algorithm)) throw std::invalid_argument(std::string(__func__) + ": null certificate");
    if (!certificate) return;
    const void* guestSignature = nullptr;
    const void* guestAlgorithm = nullptr;
    if (signature) guestSignature = ReadCertificatePointer(certificate, &LibreSsl3ValidityX509::signature, __func__);
    if (algorithm) guestAlgorithm = ReadCertificatePointer(certificate, &LibreSsl3ValidityX509::sigAlg, __func__);
    const void* previousSignature = nullptr;
    bool hasPreviousSignature = false;
    if (signature) {
        if (!CopyGuestInPreservingErrno(&previousSignature, signature, sizeof(previousSignature))) {
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        hasPreviousSignature = true;
        if (!CopyGuestOutPreservingErrno(signature, &guestSignature, sizeof(guestSignature))) {
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
    }
    if (algorithm && !CopyGuestOutPreservingErrno(algorithm, &guestAlgorithm, sizeof(guestAlgorithm))) {
        if (hasPreviousSignature) CopyGuestOutPreservingErrno(signature, &previousSignature, sizeof(previousSignature));
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
}

}
