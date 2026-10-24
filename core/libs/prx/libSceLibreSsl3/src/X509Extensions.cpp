#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSsl3Stack {
    int num;
    char** data;
    int sorted;
    int numAlloc;
    void* comp;
};

struct LibreSsl3ExtensionsCertInfo {
    unsigned char beforeExtensions[72];
    const LibreSsl3Stack* extensions;
    unsigned char rest[24];
};

struct LibreSsl3ExtensionsX509 {
    const LibreSsl3ExtensionsCertInfo* certInfo;
    unsigned char rest[208];
};

static_assert(sizeof(LibreSsl3Stack) == 32);
static_assert(offsetof(LibreSsl3Stack, data) == 8);
static_assert(sizeof(LibreSsl3ExtensionsCertInfo) == 104);
static_assert(offsetof(LibreSsl3ExtensionsCertInfo, extensions) == 72);
static_assert(sizeof(LibreSsl3ExtensionsX509) == 216);

namespace {

const LibreSsl3Stack* Extensions(const LibreSsl3ExtensionsX509* certificate, const char* function) {
    if (!certificate) throw std::invalid_argument(std::string(function) + ": null certificate");
    if (!certificate->certInfo) throw std::invalid_argument(std::string(function) + ": null certificate info");
    return certificate->certInfo->extensions;
}

}

extern "C" {

const LibreSsl3Stack* APS5_VABI X509_get0_extensions_nid_postfix(const LibreSsl3ExtensionsX509* certificate) {
    return Extensions(certificate, __func__);
}

int APS5_VABI X509_get_ext_count_nid_postfix(const LibreSsl3ExtensionsX509* certificate) {
    const LibreSsl3Stack* extensions = Extensions(certificate, __func__);
    return extensions ? extensions->num : 0;
}

void* APS5_VABI X509_get_ext_nid_postfix(const LibreSsl3ExtensionsX509* certificate, int loc) {
    const LibreSsl3Stack* extensions = Extensions(certificate, __func__);
    if (!extensions || loc < 0 || loc >= extensions->num) return nullptr;
    if (!extensions->data) throw std::invalid_argument(std::string(__func__) + ": null extension array");
    return extensions->data[loc];
}

}
