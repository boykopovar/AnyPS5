# libSceLibreSsl3

## change [AUTHORITY_KEYID_free and X509_CRL_free]

- [AUTHORITY_KEYID_free, X509_CRL_free and PKCS7_free](../../../core/libs/prx/libSceLibreSsl3/src/X509ItemFree.cpp#L6) - not measured on a PS5; non-null object disposal remains unsupported and throws before dereference. The null no-op follows LibreSSL 4.0.0 [`AUTHORITY_KEYID_free`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x509_akeya.c#L124-L128), [`X509_CRL_free`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_crl.c#L507-L511), [`PKCS7_free`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pkcs7/pk7_asn1.c#L242-L246) and their [`ASN1_item_free` dispatcher](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/tasn_fre.c#L72-L99).
