# libSceLibreSsl3

## add Functional

- [X509_print](../../../core/libs/prx/libSceLibreSsl3/src/X509Print.cpp) - prints the pinned LibreSSL `asn1/t_x509.c` `X509_print` text form from the certificate model: the `Certificate:`/`    Data:` header, the version line, the serial number in both the decimal/hexadecimal and the hexadecimal-dump form, the `    Signature Algorithm:` line, `Issuer:` and `Subject:` with the flat name built the way `X509_NAME_print` builds it from libSceLibreSsl's `X509_NAME_oneline`, `        Validity` with both times through `ASN1_TIME_print`, and the trailing signature algorithm line with the signature in the reference's 18-byte-per-line hexadecimal dump. Subject public key info details and the `X509v3 extensions:` block are omitted because the model has no `EVP_PKEY_print_public` printer and no extension printer, so the output stops after the subject line; nothing is printed in their place. RSA-PSS parameter printing and the DSA `r`/`s` signature block throw `NotImplemented_nid_no_patch`. A null BIO or certificate returns 0, a failed write returns 0, and a null serial number, signature algorithm or validity throws where LibreSSL dereferences it. Nothing is verified on a console. Closes when: compared with the console.

## remove `LA1MlgDx80o`
