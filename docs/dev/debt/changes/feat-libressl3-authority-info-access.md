# libSceLibreSsl3

## add Functional

- [`X509V3_EXT_d2i` authorityInfoAccess and subjectInfoAccess](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtD2i.cpp) (libSceLibreSsl3) - not checked on a console. Methods 177 and 398, which the `X509V3_EXT_d2i` entry lists as throwing, decode as LibreSSL 4.0.0 [`x509_info.c`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x509_info.c) into a `STACK_OF` 16-byte `ACCESS_DESCRIPTION` records (`method`, `location`) using the `GENERAL_NAME` decoder. An indefinite-length outer or element sequence and the unmodelled `GENERAL_NAME` forms throw; `ACCESS_DESCRIPTION` omits the redundant `GENERAL_NAME` pre-allocation that `ASN1_item_ex_new` makes and replaces, so its `[65, 65, 58, 58]` allocation-failure queue does not occur. This library implements no `AUTHORITY_INFO_ACCESS_free` or `ACCESS_DESCRIPTION_free`: callers release the parts themselves. Closes when: compared with the console.
