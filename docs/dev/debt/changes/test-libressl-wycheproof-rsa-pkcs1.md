# libSceLibreSsl

## add Unknown function info

- [RSA_verify ASN.1 errors](../../../core/libs/prx/libSceLibreSsl/src/Rsa.cpp) (libSceLibreSsl) - a DigestInfo that the `X509_SIG` decode rejects queues no ASN.1 error here, where LibreSSL 2.5.4 queues the errors of its template decoder (library 13, function 0xfff) before it returns 0: `ASN1_R_WRONG_TAG`, `ASN1_R_TOO_LONG`, `ASN1_R_HEADER_TOO_LONG`, `ASN1_R_BAD_OBJECT_HEADER`, `ASN1_R_SEQUENCE_LENGTH_MISMATCH` and other `ASN1_R_*` reasons, most of them followed by `ERR_R_NESTED_ASN1_ERROR` once for each enclosing template. The results and the RSA library errors match LibreSSL 2.5.4 on the 866 Wycheproof tests in [WycheproofRsaPkcs1.cpp](../../../core/libs/prx/libSceLibreSsl/tests/WycheproofRsaPkcs1.cpp) that `RSA_verify` answers; the other 168 use the BER forms that throw here.
