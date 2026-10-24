#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslAsn1Encoding {
    unsigned char* enc;
    long len;
    int modified;
};

struct LibreSslX509CertInfo {
    void* version;
    void* serialNumber;
    void* signature;
    void* issuer;
    void* validity;
    void* subject;
    void* key;
    void* issuerUid;
    void* subjectUid;
    void* extensions;
    LibreSslAsn1Encoding enc;
};

struct LibreSslX509 {
    LibreSslX509CertInfo* certInfo;
    void* sigAlg;
    void* signature;
    unsigned char rest[152];
};

static_assert(sizeof(LibreSslX509CertInfo) == 104);
static_assert(offsetof(LibreSslX509CertInfo, serialNumber) == 8);
static_assert(offsetof(LibreSslX509CertInfo, issuer) == 24);
static_assert(offsetof(LibreSslX509CertInfo, subject) == 40);
static_assert(sizeof(LibreSslX509) == 176);
static_assert(offsetof(LibreSslX509, certInfo) == 0);

namespace {

const LibreSslX509CertInfo& CertInfo(const LibreSslX509* certificate, const char* function) {
    if (!certificate) throw std::invalid_argument(std::string(function) + ": null certificate");
    if (!certificate->certInfo) throw std::invalid_argument(std::string(function) + ": null certificate info");
    return *certificate->certInfo;
}

}

extern "C" {

void* APS5_VABI X509_get_issuer_name_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).issuer;
}

}
