# libSceLibreSsl3

## add Functional

- [X509_print](../../../core/libs/prx/libSceLibreSsl3/src/X509Print.cpp) - prints LibreSSL 4.0.0 `asn1/t_x509.c`'s text form from the model: `Certificate:`/`    Data:`, the version line, the serial number in both forms, the signature algorithm line, `Issuer:` and `Subject:` as flat names, `        Validity` with both times through `ASN1_time_print`, and the trailing signature algorithm with the signature's 18-byte hex dump. The subject-public-key-info details and the `X509v3 extensions:` block are omitted because the model has no public-key or extension printer, and nothing is printed in their place; RSA-PSS parameters and the DSA `r`/`s` block throw `NotImplemented_nid_no_patch`. A null BIO or certificate returns 0, a failed write returns 0, and a null serial number, signature algorithm or validity throws. Nothing is verified on a console. Closes when: compared with the console.

## remove `LA1MlgDx80o`
