#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/libc.hpp"
#include <cstdint>

extern "C" int APS5_VABI X509_set_version_nid_postfix(LibreSsl3Certificate* certificate, std::int64_t version) {
    if (!certificate || !certificate->certInfo) return 0;
    auto* info = const_cast<LibreSsl3CertificateInfo*>(certificate->certInfo);
    auto* integer = static_cast<Asn1String*>(info->version);
    const bool created = integer == nullptr;
    if (!integer) {
        integer = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
        if (!integer) return 0;
    }
    if (!ASN1_INTEGER_set_nid_postfix(integer, version)) {
        if (created) free_nid_postfix(integer);
        return 0;
    }
    info->version = integer;
    return 1;
}
