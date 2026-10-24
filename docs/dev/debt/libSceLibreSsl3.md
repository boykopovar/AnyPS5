# libSceLibreSsl3 technical debt

### Unknown function info

- [libSceLibreSsl3 pending exports](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) - 83 named callable exports, excluding `CRYPTO_memcmp`, `MD5` and `SHA1`, remain unimplemented from the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibreSsl3.c), cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

### Functional

- [CRYPTO_memcmp](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) follows the public [LibreSSL signature and equality contract](https://man.openbsd.org/CRYPTO_memcmp.3), without PS5 verification. The exact nonzero result for unequal buffers is unspecified by that contract. Content-independent execution of its volatile byte reads still depends on the compiler; inspect optimized code when changing toolchains. Closes when: measured on a PS5 and the supported toolchains' generated code is checked.
- [MD5 and SHA1](../../../core/libs/prx/libSceLibreSsl3/src/Digest.cpp) (libSceLibreSsl3) - follow LibreSSL's [md5.c, sha1.c](https://github.com/libressl/openbsd/tree/bc9483725c5815efb64b6d9acc52fc5fba139212/src/lib/libcrypto) and hash through libSceLibreSsl's `MD5_*` and `SHA1_*`. A null digest buffer throws: LibreSSL up to 3.8 returns a static buffer, later versions do not check it. Nothing is checked on the console

- [`X509_PUBKEY_get0_param`](../../../core/libs/prx/libSceLibreSsl3/src/X509PubkeyParam.cpp) follows LibreSSL 4.0.0 `crypto/asn1/x_pubkey.c` at [`ce488b78e0bd994780569df718596b9776ff681a`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_pubkey.c) and its `x509_local.h`, `x509.h` and `asn1.h` layouts. The function returns borrowed pointers and writes the encoded key length only when the key output is requested. The PS5 layout and malformed-object behavior are not verified on hardware; layout assertions match the pinned x86-64 source declarations.
