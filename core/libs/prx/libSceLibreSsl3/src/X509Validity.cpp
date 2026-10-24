#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSsl3X509Validity {
    void* notBefore;
    void* notAfter;
};

struct LibreSsl3ValidityCertInfo {
    void* version;
    void* serialNumber;
    void* signature;
    void* issuer;
    LibreSsl3X509Validity* validity;
    unsigned char rest[64];
};

struct LibreSsl3ValidityX509 {
    LibreSsl3ValidityCertInfo* certInfo;
    const void* sigAlg;
    const void* signature;
    unsigned char rest[192];
};

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

}
