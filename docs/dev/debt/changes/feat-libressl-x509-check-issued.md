# libSceLibreSsl

## add Functional

- [X509_check_issued](../../../core/libs/prx/libSceLibreSsl/src/X509CheckIssued.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 4.0.0 `x509_purp.c`: it compares the issuer subject with the subject's issuer name entry by entry, checks the subject's authority key identifier against the issuer's subject key identifier and serial (an `authorityCertIssuer` field throws because GeneralName decoding is not modelled), and rejects an issuer key usage without `keyCertSign` (or without `digitalSignature` for a proxy certificate, detected by the `proxyCertInfo` object name). Zero means the issuer may have issued the subject, otherwise the matching `X509_V_ERR_*` code is returned. Closes when: compared with the console.
