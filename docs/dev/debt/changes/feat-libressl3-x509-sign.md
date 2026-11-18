# libSceLibreSsl3

## remove `TpBvY92mrSM`

## add Functional

- [X509_sign](../../../core/libs/prx/libSceLibreSsl3/src/X509Sign.cpp) (libSceLibreSsl3) - nothing is checked on the console. Following LibreSSL 4.0.0 [x_all.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x_all.c), it signs the cached certificate-info encoding, the TBS that `i2d_X509` also reads, and sets both algorithm identifiers from the first object-table entry whose `OBJ_find_sigid_algs` digest and key NIDs match the digest and `EVP_PKEY_base_id`, with a NULL parameter for RSA keys; the signature is stored in the certificate bit string with the no-unused-bits flag and returned. Signing goes through `EVP_SignFinal`. Unlike LibreSSL it does not mark the encoding modified or re-encode it, so a modified, absent or differently signed certificate info throws `NotImplemented_nid_no_patch`; EC keys and key types without a signature algorithm object (HMAC) throw; null arguments return 0. Closes when: compared with the console.
