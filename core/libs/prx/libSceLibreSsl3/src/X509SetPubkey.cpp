#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/libc.hpp"
#include <cstddef>

namespace {
struct CertInfo {
    void* version;
    void* serial;
    void* signature;
    void* issuer;
    LibreSsl3X509Validity* validity;
    void* subject;
    LibreSsl3X509Pubkey* key;
};
static_assert(offsetof(CertInfo, key) == 48);
}

extern "C" int APS5_VABI X509_set_pubkey_nid_postfix(LibreSsl3X509* certificate, LibreSsl3EvpPkey* key) {
    if (!certificate || !certificate->certInfo || !key) return 0;
    auto* stored = static_cast<LibreSsl3X509Pubkey*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Pubkey)));
    if (!stored) return 0;
    EVP_PKEY_up_ref_nid_postfix(key);
    stored->pkey = key;
    reinterpret_cast<CertInfo*>(certificate->certInfo)->key = stored;
    return 1;
}
