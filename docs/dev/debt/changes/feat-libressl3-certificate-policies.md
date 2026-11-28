# libSceLibreSsl3

## add Functional

- [`X509V3_EXT_d2i` certificatePolicies](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtD2i.cpp) (libSceLibreSsl3) - not checked on a console. Method 89 decodes as LibreSSL 4.0.0 [`x509_cpols.c`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x509_cpols.c) into a `STACK_OF` 16-byte `POLICYINFO` records (`policyid`, `qualifiers` of `POLICYQUALINFO` for `id-qt-cps`, `id-qt-unotice`, and `ASN1_ANY` `other`). Indefinite-length outer or nested structure sequences (`CERTIFICATEPOLICIES`, `POLICYINFO`, `qualifiers`, `POLICYQUALINFO`, `USERNOTICE`, `NOTICEREF`, `noticenos`) throw. This library implements no `CERTIFICATEPOLICIES_free`, `POLICYINFO_free`, `POLICYQUALINFO_free`, `USERNOTICE_free`, or `NOTICEREF_free`: callers release the parts themselves. Closes when: compared with the console.
