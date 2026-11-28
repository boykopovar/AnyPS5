# libSceLibreSsl3

## change EVP_get_digestbyname

- [EVP_get_digestbyname](../../../core/libs/prx/libSceLibreSsl3/src/DigestNames.cpp) (libSceLibreSsl3) - nothing is checked on the console. Each of the 91 names returns the existing `EVP_md5`, `EVP_sha1`, `EVP_sha224`, `EVP_sha256`, `EVP_sha384`, `EVP_sha512` or `EVP_sha512_256` object or one of the [internal digests](../../../core/libs/prx/libSceLibreSsl3/src/ExtraDigests.cpp). The table, its exact `strcmp` lookup and the null result for a null or unknown name follow [LibreSSL 4.0.0](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/evp_names.c) `evp_names.c`, with the names taken from `objects.txt`. Its `OPENSSL_init_crypto` call is left out: it fails only after `OPENSSL_cleanup`, which neither library exports. Closes when: the names are compared with the console.
