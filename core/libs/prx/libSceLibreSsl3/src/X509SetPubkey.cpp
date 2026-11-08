#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/libc.hpp"
#include <cstddef>
#include <stdexcept>

namespace {

struct CertInfo {
    void* version;
    void* serialNumber;
    void* signature;
    void* issuer;
    LibreSsl3X509Validity* validity;
    void* subject;
    LibreSsl3X509Pubkey* key;
};

static_assert(offsetof(CertInfo, key) == 48);

}

extern "C" int APS5_VABI X509_set_pubkey_nid_postfix(LibreSsl3X509* certificate, LibreSsl3EvpPkey* key) {
    if (!certificate || !certificate->certInfo)
        return 0;
    if (!key) return 0;
    auto* info = reinterpret_cast<CertInfo*>(certificate->certInfo);
    auto* stored = static_cast<LibreSsl3X509Pubkey*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Pubkey)));
    if (!stored) return 0;
    stored->pkey = key;
    info->key = stored;
    return 1;
}
