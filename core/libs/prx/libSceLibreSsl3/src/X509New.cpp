#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/libc.hpp"

extern "C" LibreSsl3Certificate* APS5_VABI X509_new_nid_postfix() {
    auto* certificate = static_cast<LibreSsl3Certificate*>(calloc_nid_postfix(1, sizeof(LibreSsl3Certificate)));
    if (!certificate) return nullptr;
    auto* info = static_cast<LibreSsl3CertificateInfo*>(calloc_nid_postfix(1, sizeof(LibreSsl3CertificateInfo)));
    if (!info) {
        free_nid_postfix(certificate);
        return nullptr;
    }
    certificate->certInfo = info;
    reinterpret_cast<LibreSsl3X509*>(certificate)->references = 1;
    return certificate;
}
