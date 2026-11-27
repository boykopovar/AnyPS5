# libSceLibreSsl3

## change [EVP_EncryptFinal_ex, EVP_DecryptFinal_ex and EVP_CipherFinal_ex]

- [EVP_EncryptFinal_ex, EVP_DecryptFinal_ex and EVP_CipherFinal_ex](../../../core/libs/prx/libSceLibreSsl3/src/EvpCipherFinal.cpp) (libSceLibreSsl3) - nothing is checked on the console, and the NID binding gap of the updates applies. They follow [LibreSSL 4.0.0](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/evp_cipher.c) `evp_cipher.c` over libSceLibreSsl's 2.5.4 `EVP_CIPHER_CTX` and queue their errors in the libSceLibreSsl queue. A null, unreadable or read-only context, a null or unreadable cipher, a null or unwritable `outl`, a cipher without `do_cipher`, a null or unwritable output for a padded block or plaintext, and a held block with block size 0, which LibreSSL dereferences or reads out of bounds, throw. Closes when: compared with the console.
