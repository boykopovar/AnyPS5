# libSceLibreSsl

## change [EVP_aes_128_wrap, EVP_aes_192_wrap, EVP_aes_256_wrap]

- [EVP_aes_128_wrap, EVP_aes_192_wrap, EVP_aes_256_wrap](../../../core/libs/prx/libSceLibreSsl/src/EvpAes.cpp) (libSceLibreSsl) - nothing is checked on the console. LibreSSL 2.5.4 has no AES key wrap, so the RFC 3394 loops, the descriptors and the wrap-allow gate follow the [e_aes.c](https://github.com/libressl/openbsd/blob/9651ec05f00bf3384031fb7428d88d3daf1e1139/src/lib/libcrypto/evp/e_aes.c) of the LibreSSL commit that added it (`WRAP_FLAGS` with `EVP_CIPH_WRAP_MODE` 0x10002, `aes_wrap_init_key`, `aes_wrap_cipher`); NIDs 788 to 790 are those of 2.5.4's [obj_mac.num](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/objects/obj_mac.num). The context uses the port's `AesKey` layout with the IV inline and no `EVP_CTRL_COPY` handler, so a title reading it sees different bytes. Bad lengths and failed unwrap integrity return -1.
