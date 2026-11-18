# libSceLibreSsl3

## add Functional

- [d2i_RSA_PUBKEY](../../../core/libs/prx/libSceLibreSsl3/src/KeyDer.cpp) (libSceLibreSsl3) - nothing is checked on the console. It follows `d2i_RSA_PUBKEY` and `pubkey_ex_d2i` in LibreSSL 4.0.0 [x_pubkey.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_pubkey.c#L254-L305): the SubjectPublicKeyInfo is decoded with this module's `d2i_PUBKEY`, the key is taken with `EVP_PKEY_get1_RSA`, and only a decoded key frees and replaces `*out` and advances `*in`. A negative length returns null without an error. It inherits the decoding gaps of `d2i_PUBKEY`, including its `ASN1_R_BAD_OBJECT_HEADER` for a null `*in`. A null `in`, which LibreSSL dereferences, throws. Closes when: compared with the console.

## change [RSA_get0_key, DH_get0_pqg, DH_get0_key, DSA_get0_pqg, DSA_get0_key]

- [RSA_get0_key, DH_get0_pqg, DH_get0_key, DSA_get0_pqg, DSA_get0_key](../../../core/libs/prx/libSceLibreSsl3/src/KeyComponents.cpp) (libSceLibreSsl3) - `RSA_new`, `d2i_RSA_PUBKEY` and `PEM_read_RSAPrivateKey` are exported by this library, so their imports no longer [bind to libSceLibreSsl](https://github.com/boykopovar/AnyPS5/pull/10490#pullrequestreview-5644839495)'s 2.5.4 layout; `PEM_read_RSAPrivateKey` still throws. `RSA_set0_key`, exported by firmware 13.42, is implemented in `RsaSetters.cpp` without console verification. The layouts follow [LibreSSL 4.0.0](https://github.com/libressl/openbsd/tree/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto) `rsa_local.h`, `dh_local.h` and `dsa_local.h`, as [firmware 13.42's module run on a PC](https://github.com/boykopovar/AnyPS5/pull/10490#issuecomment-6405344642) uses. A null key throws when an output is requested, where LibreSSL dereferences it
