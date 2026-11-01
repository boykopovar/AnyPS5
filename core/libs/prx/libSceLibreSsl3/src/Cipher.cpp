#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

extern "C" {

int APS5_VABI EVP_CIPHER_iv_length_nid_postfix(const LibreSsl3EvpCipher* cipher) {
    if (!cipher)
        throw std::invalid_argument("EVP_CIPHER_iv_length requires a cipher");
    return cipher->ivLength;
}

int APS5_VABI EVP_CIPHER_key_length_nid_postfix(const LibreSsl3EvpCipher* cipher) {
    if (!cipher)
        throw std::invalid_argument("EVP_CIPHER_key_length requires a cipher");
    return cipher->keyLength;
}

}
