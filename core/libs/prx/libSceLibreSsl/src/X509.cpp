#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslX509CertInfo) == 104);
static_assert(offsetof(LibreSslX509CertInfo, serialNumber) == 8);
static_assert(offsetof(LibreSslX509CertInfo, issuer) == 24);
static_assert(offsetof(LibreSslX509CertInfo, subject) == 40);
static_assert(offsetof(LibreSslX509, certificateInfo) == 0);

namespace {

const LibreSslX509CertInfo& CertInfo(const LibreSslX509* certificate, const char* function) {
    if (!certificate) throw std::invalid_argument(std::string(function) + ": null certificate");
    if (!certificate->certificateInfo) throw std::invalid_argument(std::string(function) + ": null certificate info");
    return *static_cast<const LibreSslX509CertInfo*>(certificate->certificateInfo);
}

}

extern "C" {

void* APS5_VABI X509_get_issuer_name_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).issuer;
}

void* APS5_VABI X509_get_subject_name_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).subject;
}

void* APS5_VABI X509_get_serialNumber_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).serialNumber;
}

}
