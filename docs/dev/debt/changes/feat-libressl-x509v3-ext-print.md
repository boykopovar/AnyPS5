# libSceLibreSsl

## add Functional

- [X509V3_EXT_print](../../../core/libs/prx/libSceLibreSsl/src/X509V3Print.cpp) (libSceLibreSsl) - not measured on a PS5. It follows LibreSSL 2.5.4 [v3_prn.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/x509v3/v3_prn.c#L103-L211) and the [v3_ocsp.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/x509v3/v3_ocsp.c#L215-L344) nonce, CRL ID, archive cutoff, invalidity date, no-check and hold instruction printers; unknown extensions take the default, error and dump paths. Other `ext_dat.h` methods (certificate extensions, OCSP service locator, acceptable responses) and `X509V3_EXT_PARSE_UNKNOWN` throw. A null extension, a null, negative-length or null-data value LibreSSL reads, indent `INT_MIN` and a dump indent of 67 to 70 (a division by zero in `b_dump.c`) throw.
