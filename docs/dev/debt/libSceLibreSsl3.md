# libSceLibreSsl3 technical debt

### Unknown function info

- [libSceLibreSsl3 pending exports](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) - 84 named callable exports, excluding `CRYPTO_memcmp`, `MD5` and `SHA1`, remain unimplemented from the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibreSsl3.c), cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

### Functional

- [CRYPTO_memcmp](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) follows the public [LibreSSL signature and equality contract](https://man.openbsd.org/CRYPTO_memcmp.3), without PS5 verification. The exact nonzero result for unequal buffers is unspecified by that contract. Content-independent execution of its volatile byte reads still depends on the compiler; inspect optimized code when changing toolchains. Closes when: measured on a PS5 and the supported toolchains' generated code is checked.
- [MD5 and SHA1](../../../core/libs/prx/libSceLibreSsl3/src/Digest.cpp) (libSceLibreSsl3) - follow LibreSSL's [md5.c, sha1.c](https://github.com/libressl/openbsd/tree/bc9483725c5815efb64b6d9acc52fc5fba139212/src/lib/libcrypto) and hash through libSceLibreSsl's `MD5_*` and `SHA1_*`. A null digest buffer throws: LibreSSL up to 3.8 returns a static buffer, later versions do not check it. Nothing is checked on the console
