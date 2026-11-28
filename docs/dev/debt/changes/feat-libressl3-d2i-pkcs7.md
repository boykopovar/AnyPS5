# libSceLibreSsl3

## add Functional

- [d2i_PKCS7 and PKCS7_free](../../../core/libs/prx/libSceLibreSsl3/src/Pkcs7.cpp) (libSceLibreSsl3) - not checked on the console. Enveloped, signed-and-enveloped and encrypted content, indefinite lengths, constructed strings, high tag numbers and a null `in` throw; UTCTime and GeneralizedTime values are not validated. `data`, `signed`, `digest` and unknown content types follow the LibreSSL 4.0.0 [pk7_asn1.c templates](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pkcs7/pk7_asn1.c#L77-L489) and [pkcs7.h layouts](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pkcs7/pkcs7.h#L84-L198); certificates use the `X509_STORE_load_locations` decoder. Closes when: the other content types are decoded and compared with the console.
