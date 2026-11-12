#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

extern "C" void* APS5_VABI X509_get0_pubkey_nid_postfix(const LibreSsl3Certificate* certificate) {
    if (!certificate || !certificate->certInfo) return nullptr;
    auto* key = certificate->certInfo->key;
    if (!key) return nullptr;
    if (!key->pkey && key->publicKey) NotImplemented_nid_no_patch("X509_get0_pubkey");
    return key->pkey;
}
