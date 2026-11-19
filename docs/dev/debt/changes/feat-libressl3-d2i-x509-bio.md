# libSceLibreSsl3

## add Functional

- [`d2i_X509_bio`](../../../core/libs/prx/libSceLibreSsl3/src/X509Bio.cpp) reads one definite or indefinite BER object through the legacy BIO ABI and passes it to libSceLibreSsl's X509 decoder. Its buffering, callback, byte-count and error behavior follows LibreSSL 4.0.0 [`a_d2i_fp.c`](https://github.com/libressl/openbsd/blob/078b3d1df8ca2011c1605cd40418c58fa0f5b143/src/lib/libcrypto/asn1/a_d2i_fp.c); it can read ahead by up to a header when finishing an indefinite object. The PS5 signature, BIO and X509 layouts and console behavior are unverified. Inaccessible BIO state or output storage throws instead of faulting. Closes when: compared with the console.
