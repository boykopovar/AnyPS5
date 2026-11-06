#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

const LibreSsl3X509Pubkey* CheckedPubkey(const void* pub, const char* function) {
    if (!pub) throw std::invalid_argument(std::string(function) + ": null public key");
    return static_cast<const LibreSsl3X509Pubkey*>(pub);
}

}

extern "C" {

int APS5_VABI X509_PUBKEY_get0_param_nid_postfix(void** publicKeyAlgorithm,
    const unsigned char** publicKey, int* publicKeyLength, void** algorithm,
    const void* pub) {
    if (publicKeyAlgorithm) {
        const auto* key = CheckedPubkey(pub, __func__);
        if (!key->algor) throw std::invalid_argument(std::string(__func__) + ": null algorithm info");
        *publicKeyAlgorithm = key->algor->algorithm;
    }
    if (publicKey) {
        const auto* key = CheckedPubkey(pub, __func__);
        if (!key->publicKey) throw std::invalid_argument(std::string(__func__) + ": null encoded public key");
        if (!publicKeyLength) throw std::invalid_argument(std::string(__func__) + ": null public key length");
        *publicKey = key->publicKey->data;
        *publicKeyLength = key->publicKey->length;
    }
    if (algorithm) {
        const auto* key = CheckedPubkey(pub, __func__);
        *algorithm = key->algor;
    }
    return 1;
}

}
