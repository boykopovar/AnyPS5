# libSceLibreSsl3

## remove `TpBvY92mrSM`

## add Functional

- [X509_sign](../../../core/libs/prx/libSceLibreSsl3/src/X509Sign.cpp) (libSceLibreSsl3) - re-encodes only the signature AlgorithmIdentifier of the cached certificate-info encoding (TBS), where LibreSSL 4.0.0 `x_all.c` and `asn1_item.c` re-encode the TBS from its fields: a field changed in place without the modified flag (`X509_add_ext`, a time from `X509_getm_notAfter`) or a cached BIT STRING with non-zero padding bits (public key, unique IDs), which LibreSSL clears, is signed as cached. A modified, absent or differently signed TBS throws `NotImplemented_nid_no_patch`. The rebuilt TBS is stored as an unmodified cache, where LibreSSL leaves it marked modified. Both identifiers take the first object-table entry whose `OBJ_find_sigid_algs` NIDs match the digest and `EVP_PKEY_base_id`, with a NULL parameter for RSA keys. RSA-PSS, EC, Ed25519, HMAC and key aliases throw or pick the aliased NID; null arguments return 0. No console check. Closes when: compared with the console.
