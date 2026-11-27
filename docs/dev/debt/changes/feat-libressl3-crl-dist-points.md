# libSceLibreSsl3

## add Functional

- [`X509V3_EXT_d2i` cRLDistributionPoints and freshestCRL](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtD2i.cpp) (libSceLibreSsl3) - not checked on a console. Methods 103 and 857, which the `X509V3_EXT_d2i` entry lists as throwing, decode as LibreSSL 4.0.0 [`x509_crld.c`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x509_crld.c) into a `STACK_OF` 32-byte `DIST_POINT` records (`dp_reasons` 0, `dpname` null) reusing the `GENERAL_NAME` decoder. Indefinite-length sequences or fields, constructed `reasons` bits, constructed `X509_NAME_ENTRY` string values, and the unmodelled `GENERAL_NAME` forms throw. This library implements no `CRL_DIST_POINTS_free`, `DIST_POINT_free`, or `DIST_POINT_NAME_free`: callers release the parts themselves. Closes when: compared with the console.
