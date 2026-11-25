# libSceLibreSsl3

## change [AUTHORITY_KEYID_free and X509_CRL_free]

- [AUTHORITY_KEYID_free and X509_CRL_free](../../../core/libs/prx/libSceLibreSsl3/src/X509ItemFree.cpp#L6) - not measured on a PS5. `AUTHORITY_KEYID_free` releases the key identifier, issuer names, serial number and 24-byte owner using the layouts and destructors already used by `X509V3_EXT_d2i`, following LibreSSL 4.0.0 [`AUTHORITY_KEYID_free`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x509_akeya.c#L124-L128); an unreadable owner throws as a host-safety fallback. `X509_CRL_free` frees the decoded CRL model described by its own entry.
