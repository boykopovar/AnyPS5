# libSceLibreSsl

## add Unknown function info

- [d2i_RSA_PUBKEY on Windows](../../../core/libs/prx/libSceLibreSsl/src/Rsa.cpp) (libSceLibreSsl) - a DER length of 2^31 bytes or more is refused on Windows, where the parser keeps element lengths in the 32-bit host `long`. [LibreSSL 2.5.4](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/asn1/asn1_lib.c) with the console's 64-bit `long` accepts lengths up to `LONG_MAX`; not measured on a console.
