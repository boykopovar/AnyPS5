# libSceLibreSsl

## add Functional

- [X509_free](../../../core/libs/prx/libSceLibreSsl/src/X509.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 2.5.4 `x509_lib.c`: the last reference releases the certificate's ASN.1 strings, names, cached encoding and the certificate itself; a retained reference only drops the count. The parent model has no layout for the signature algorithm, validity, public key or extension stack, so a certificate carrying any of them throws before the count drops, and a name with entries or cached bytes throws as well. Closes when: compared with the console.
