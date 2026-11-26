# libSceLibreSsl

## add Functional

- [X509_INFO_free](../../../core/libs/prx/libSceLibreSsl/src/X509Info.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 4.0.0 `asn1/x_info.c`: the last reference releases the certificate, the encrypted data and the info structure; the certificate is freed from the modelled fields and throws for a signature algorithm, validity, public key or extension stack, and objects carrying a CRL or private key throw before the count drops. The cipher info is borrowed. Closes when: compared with the console.
