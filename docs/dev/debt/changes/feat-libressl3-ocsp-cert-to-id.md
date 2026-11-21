# libSceLibreSsl3

## add Functional

- [OCSP_cert_to_id](../../../core/libs/prx/libSceLibreSsl3/src/OcspCertToId.cpp) (libSceLibreSsl3) - nothing is checked on the console. Re-encoding a modified issuer name throws, as do the inputs on which LibreSSL dereferences a null pointer (both certificates, a certificate info, the issuer public key, the issuer name or its encoding) and a negative issuer key length. A failed allocation releases the partial `OCSP_CERTID` and queues one `ERR_R_MALLOC_FAILURE`, where `ASN1_item_new` leaks it and queues one per nesting level. Otherwise it follows `OCSP_cert_to_id` and `OCSP_cert_id_new` in LibreSSL 4.0.0 [ocsp_lib.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/ocsp/ocsp_lib.c), hashing the cached issuer name encoding and the issuer key bits with the forwarded libSceLibreSsl `EVP_Digest`. Closes when: compared with the console.
