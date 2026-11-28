#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

extern "C" {

int APS5_VABI EVP_EncryptInit_nid_postfix(LibreSslEvpCipherCtx* context, const LibreSslEvpCipher* cipher, const unsigned char* key, const unsigned char* iv) {
    if (cipher) EVP_CIPHER_CTX_init_nid_postfix(context);
    return EVP_CipherInit_ex_nid_postfix(context, cipher, nullptr, key, iv, 1);
}

}
