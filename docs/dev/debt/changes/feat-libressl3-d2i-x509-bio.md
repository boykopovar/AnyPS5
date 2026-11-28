# libSceLibreSsl3

## add Functional

- [`d2i_X509_bio`](../../../core/libs/prx/libSceLibreSsl3/src/X509Bio.cpp) reads one definite or indefinite BER object through the legacy BIO ABI and passes it to libSceLibreSsl's X509 decoder. Its buffering, callback, byte-count and error behavior follows the OpenBSD [`a_d2i_fp.c`](https://github.com/libressl/openbsd/blob/078b3d1df8ca2011c1605cd40418c58fa0f5b143/src/lib/libcrypto/asn1/a_d2i_fp.c) revision at commit `078b3d1`; it can read ahead by up to a header when finishing an indefinite object. The PS5 signature, BIO and X509 layouts and console behavior are unverified. Inaccessible BIO state or output storage throws instead of faulting. Closes when: compared with the console.

# libSceLibreSsl

## add Unknown function info

- [d2i_X509 and X509_free](../../../core/libs/prx/libSceLibreSsl/src/X509.cpp) (libSceLibreSsl) - unverified on PS5. The `Field=` and `Type=` text LibreSSL attaches to decoding errors is not kept, as the error queue holds codes only. Freeing a certificate with ex_data, auxiliary data, cached extensions or a decoded public key throws; no export creates them yet. A null input pointer, or a null input with a nonzero length, throws where LibreSSL faults. The rest ports the [LibreSSL 2.5.4](https://github.com/libressl/openbsd/tree/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/asn1) template decoder and the `X509` items of `x_x509.c`, `x_name.c`, `x_pubkey.c`, `x_algor.c`, `x_val.c` and `x_exten.c` over the 176-byte `X509`; `guest_libressl_oracle` (`ANYPS5_LIBRESSL_ORACLE`) compares fields, consumed length and error codes with LibreSSL 2.5.4 on generated and mutated certificates, on hosts whose `long` is 8 bytes as in the guest
