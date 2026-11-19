# libSceLibreSsl3

## remove `X509_check_akid`

## add Functional

- [X509_check_akid](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp#L77) - not measured on a PS5. A null authority key identifier returns `X509_V_OK` (0) without reading the issuer, as in LibreSSL 4.0.0 [`X509_check_akid`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x509_purp.c#L853-L890). A non-null identifier throws before dereference, because no export creates an `AUTHORITY_KEYID` yet; comparing its key ID, serial number and issuer name against the certificate is unimplemented.
