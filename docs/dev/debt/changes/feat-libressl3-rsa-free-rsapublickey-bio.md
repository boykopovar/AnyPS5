# libSceLibreSsl3

## add Functional

- [RSA_free](../../../core/libs/prx/libSceLibreSsl3/src/RsaCrypto.cpp) (libSceLibreSsl3) - not measured on a PS5. A key whose method pointer is set goes to libSceLibreSsl's `RSA_free`, because only that library's `RSA_new` sets one; keys from this module's `RSA_new`, which leaves it null, are freed here. Native keys holding PSS parameters, blinding state, extra data or method data still throw. Otherwise follows LibreSSL 4.0.0 [`RSA_free`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/rsa/rsa_lib.c#L160-L189). Closes when: the RSA layout is measured on the firmware module and native keys carry a method.
- [d2i_RSAPublicKey_bio](../../../core/libs/prx/libSceLibreSsl3/src/RsaPublicKeyBio.cpp) (libSceLibreSsl3) - not measured on a PS5. Follows LibreSSL 4.0.0 [`d2i_RSAPublicKey_bio`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x_all.c#L191-L195), the [SEQUENCE decoder](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/tasn_dec.c#L735-L909) and [`c2i_ASN1_INTEGER_cbs`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/a_int.c#L660-L739), including BER headers inside the SEQUENCE and negative integers. The `d2i_PrivateKey_bio` reader fetches the object and rejects the indefinite outer lengths that `asn1_d2i_read_bio` accepts. `Field=` and `Type=` error data is not queued. Closes when: the module is measured and the reader accepts indefinite outer lengths.

## remove [`d2i_RSA_PUBKEY` and `d2i_RSAPublicKey_bio`]

## remove [`PEM_read_RSAPrivateKey`]

## change [`PEM_read_bio_PrivateKey`, `PEM_read_bio_PUBKEY` and `PEM_write_bio_PrivateKey`]

- [`PEM_read_bio_PrivateKey`, `PEM_read_bio_PUBKEY` and `PEM_write_bio_PrivateKey`](../../../core/libs/prx/libSceLibreSsl3/src/PemKeys.cpp) (libSceLibreSsl3) - encrypted keys stay unimplemented: `ENCRYPTED PRIVATE KEY`, a well-formed `Proc-Type: 4,ENCRYPTED` plus `DEK-Info` header and a non-null cipher in the writer throw through `NotImplemented_nid_no_patch`, so the cipher lookup, `PEM_do_header` and password callbacks have no behaviour. A `DSA PRIVATE KEY` record throws the same way. Other headers follow `PEM_get_EVP_CIPHER_INFO`. Bare `RSA*` labels stay stubs. The `EVP_PKEY_ASN1_METHOD` table is a static list of the 4.0.0 `pem_str` entries, and writes are always PKCS#8. `PEM_read_bio_PrivateKey` keeps its NID behind a `nid_disambig1` name because the parent declares the plain one. The `Expecting: %s` error data is not queued; no console measurement. Closes when: encrypted keys, DSA and bare `RSA*` labels land.
