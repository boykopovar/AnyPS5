# libSceLibreSsl3

## remove `X509_check_akid`

## add Functional

- [X509_check_akid](../../../core/libs/prx/libSceLibreSsl3/src/X509CheckAkid.cpp) - not measured on a PS5. An issuer name whose `modified` flag is set throws, because re-encoding an `X509_NAME` is not implemented; LibreSSL 4.0.0 re-encodes it before comparing. The key identifier is compared only with the `skid` cached at offset 96 of the certificate, which no export of this library fills yet, so the check is skipped for the certificates it decodes, as LibreSSL skips it before `x509v3_cache_extensions` has run. Otherwise the key identifier, the serial number (sign first) and the first directory name of the issuer names are compared as in LibreSSL 4.0.0 [`X509_check_akid`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x509_purp.c#L853-L890), returning `X509_V_ERR_AKID_SKID_MISMATCH` (30) or `X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH` (31); a null or unreadable field that LibreSSL would dereference throws.
