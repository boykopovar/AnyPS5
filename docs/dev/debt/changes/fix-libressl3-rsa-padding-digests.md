# libSceLibreSsl3

## add Functional

- [RSA signature digests of EVP_PKEY_CTX_ctrl](../../../core/libs/prx/libSceLibreSsl3/src/PkeyContext.cpp) (libSceLibreSsl3) - nothing is checked on the console. `EVP_PKEY_CTRL_MD` and `EVP_PKEY_CTRL_RSA_PADDING` accept the 15 digest types of LibreSSL 4.0.0 [`check_padding_md`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/rsa/rsa_pmeth.c#L402-L446). PKCS#1 v1.5 signing and verification take the `DigestInfo` OID from a nine-digest table rather than `OBJ_nid2obj`: SHA-512/256 is in it, SHA-512/224 and the four SHA-3 digests are not, so with that padding a digest method of one of those five types passes the control and then throws through `NotImplemented_nid_no_patch` when it signs or verifies. No export of this library returns such a method. X9.31 padding with a digest throws as before. Closes when: those five digests exist and the results are compared with the console.
