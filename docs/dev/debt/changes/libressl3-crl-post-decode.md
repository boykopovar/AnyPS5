# libSceLibreSsl3

## add Functional

- [d2i_X509_CRL](../../../core/libs/prx/libSceLibreSsl3/src/X509Crl.cpp) (libSceLibreSsl3) - not measured on a PS5. A CRL with exactly one issuing distribution point extension throws, because that extension is not decoded. A certificate issuer CRL entry extension is not decoded: the entry's issuer and the CRL's issuer list stay null, and a malformed one does not set `EXFLAG_INVALID`. Hashing throws for a signature algorithm with an INTEGER, BOOLEAN or BIT STRING parameter, since its re-encoding is not rebuilt. The revoked stack gets no comparison function. The hash, cached extensions, reasons and flags otherwise follow LibreSSL 4.0.0 [`crl_set_issuers` and `crl_cb`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/x_crl.c#L189-L340).
