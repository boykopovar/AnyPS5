# libSceLibreSsl3

## add Functional

- [RSA keys in `d2i_PUBKEY`](../../../core/libs/prx/libSceLibreSsl3/src/KeyDer.cpp) (libSceLibreSsl3) - not compared with the console. For `rsaEncryption` the `AlgorithmIdentifier` parameter may be absent, `NULL`, an OID or a `SEQUENCE`, and bytes after the `RSAPublicKey` in the BIT STRING are left unread, as LibreSSL 4.0.0 [`rsa_pub_decode`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/rsa/rsa_ameth.c#L168-L191) does. Still refused where 4.0.0 accepts them: a parameter of any other `ANY` type, a BIT STRING with 1 to 7 unused bits (4.0.0 masks the last byte) and the RSA-PSS OID. A refused parameter also queues `X509_R_PUBLIC_KEY_DECODE_ERROR`, which 4.0.0 raises only after the `X509_PUBKEY` has decoded. Closes when: the parameter follows the `ANY` rules of `tasn_dec.c`.
