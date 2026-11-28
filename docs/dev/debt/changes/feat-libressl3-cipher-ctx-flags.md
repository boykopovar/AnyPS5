# libSceLibreSsl3

## add Functional

- [EVP_CIPHER_CTX_set_flags](../../../core/libs/prx/libSceLibreSsl3/src/EvpCipherCtxFlags.cpp) (libSceLibreSsl3) - nothing is checked on the console. ORs the mask into the guest cipher context's flags word, as LibreSSL 2.5.4 `evp_lib.c` does, and returns 1 because the PS5 signature is unverified while LibreSSL's function returns void. A null or unreadable context throws, where LibreSSL dereferences it. Closes when: compared with the console.
