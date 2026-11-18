# libSceLibreSsl3

## remove | `X509_digest` | `k1AHUWet-oU` |

## add Functional

- [X509_digest](../../../core/libs/prx/libSceLibreSsl3/src/X509Digest.cpp) (libSceLibreSsl3) - nothing is checked on the console. It follows `X509_digest` and `ASN1_item_digest` in LibreSSL 4.0.0 [x_all.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x_all.c) and [asn1_item.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/asn1_item.c): the certificate is encoded with `i2d_X509` into a guest allocation, hashed with the forwarded libSceLibreSsl `EVP_Digest` and freed, returning 0 when nothing was encoded or the digest fails. It inherits the gaps of `i2d_X509`, whose throws it passes on after freeing the encoding. Closes when: compared with the console.
