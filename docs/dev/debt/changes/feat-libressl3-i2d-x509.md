# libSceLibreSsl3

## remove | `i2d_X509` | `e2ReciVG21w` |

## add Functional

- [i2d_X509](../../../core/libs/prx/libSceLibreSsl3/src/X509Der.cpp) (libSceLibreSsl3) - nothing is checked on the console. Re-encoding a modified certificate info throws, as do INTEGER, ENUMERATED and other unlisted algorithm parameter types; NULL, OBJECT, BOOLEAN, BIT STRING, string and raw SEQUENCE, SET and OTHER parameters are encoded. The trailing zero bytes of a BIT STRING, which LibreSSL leaves unwritten, are written as zero. Otherwise it follows `ASN1_item_i2d` with `X509_it` in LibreSSL 4.0.0 [tasn_enc.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/tasn_enc.c), including the null, length-only, allocating and advancing output forms and the `X509_CINF` encoding cache at offset 80.
