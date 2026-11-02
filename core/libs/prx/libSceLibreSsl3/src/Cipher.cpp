#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

extern "C" {

std::uint64_t APS5_VABI SSL_CIPHER_get_id_nid_postfix(const LibreSsl3Cipher* cipher) {
    if (!cipher)
        return 0;
    return cipher->id;
}

int APS5_VABI EVP_CIPHER_iv_length_nid_postfix(const LibreSsl3EvpCipher* cipher) {
    if (!cipher)
        throw std::invalid_argument("EVP_CIPHER_iv_length requires a cipher");
    return cipher->ivLength;
}

void APS5_VABI EVP_CIPHER_CTX_set_flags_nid_postfix(LibreSslEvpCipherCtx* context, int flags) {
    if (!context)
        throw std::invalid_argument("EVP_CIPHER_CTX_set_flags requires a context");
    context->flags |= static_cast<std::uint64_t>(flags);
}

int APS5_VABI EVP_CIPHER_key_length_nid_postfix(const LibreSsl3EvpCipher* cipher) {
    if (!cipher)
        throw std::invalid_argument("EVP_CIPHER_key_length requires a cipher");
    return cipher->keyLength;
}

}
