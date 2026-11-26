#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstdint>

namespace {

struct PubkeyLayout {
    void* algor;
    Asn1String* publicKey;
    void* pkey;
};

struct PkeyReferences {
    std::int32_t type;
    std::int32_t saveType;
    std::int32_t references;
};

}

extern "C" {

void* APS5_VABI X509_get_pubkey_nid_postfix(const LibreSslX509* certificate) {
    if (!certificate) return nullptr;
    const auto* info = static_cast<const LibreSslX509CertInfo*>(certificate->certificateInfo);
    if (!info || !info->key) return nullptr;
    auto* publicKey = static_cast<const PubkeyLayout*>(info->key);
    if (!publicKey->pkey) {
        if (!publicKey->algor && !publicKey->publicKey) return nullptr;
        NotImplemented_nid_no_patch("X509_get_pubkey: a certificate whose public key is not decoded");
    }
    auto* pkey = publicKey->pkey;
    std::atomic_ref<std::int32_t>(static_cast<PkeyReferences*>(pkey)->references).fetch_add(1);
    return pkey;
}

}
