# libSceLibreSsl3

## add Functional

- [X509_CRL decoding, extension lookup and revocation lookup](../../../core/libs/prx/libSceLibreSsl3/src/X509Crl.cpp) (libSceLibreSsl3) - `d2i_X509_CRL`, `X509_CRL_get_ext_d2i` and `X509_CRL_get0_by_cert` follow LibreSSL 4.0.0 `asn1/x_crl.c`, `x509/x509_lib.c` and `x509/x509_ext.c`. The model assumes the LP64 `X509_CRL` (144 bytes), `X509_CRL_INFO` (80 bytes) and `X509_REVOKED` (40 bytes) layouts; no console layout is measured. Signature algorithms with constructed parameters throw, and `certificateIssuer` entry issuers, the SHA-1 hash, `EXFLAG_*` and `crl->meth` are not populated. `X509_CRL_free` frees a decoded CRL and honours its reference count, so `d2i_X509_CRL` replaces a non-null output. Closes when: compared with the console.

## remove `fVos2kAqfMI`

## remove `je-ydGAbgFg`

## remove `LdGe2IfIlpI`
