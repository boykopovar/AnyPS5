# libSceLibreSsl3

## add Functional

- [X509_STORE_load_locations](../../../core/libs/prx/libSceLibreSsl3/src/X509StoreLoadLocations.cpp) (libSceLibreSsl3) - nothing is checked on the console. It follows LibreSSL 4.0.0 [x509_d2.c, x509_lu.c, by_file.c and by_dir.c](https://github.com/libressl/openbsd/tree/078b3d1df8ca2011c1605cd40418c58fa0f5b143/src/lib/libcrypto/x509): a `file` PEM bundle is read through the guest libc and every `CERTIFICATE`, `X509 CERTIFICATE` or `TRUSTED CERTIFICATE` record decoded and added, other records skipped; a `dir` value is a colon-separated directory list recorded on a hash-dir lookup whose by-subject control loads the reference's `<dir>/<8-hex name hash>.<n>` files lazily. The store gained `X509_OBJECT` entries, a real `X509_STORE_add_cert` with refcounting and dedupe, and a subject-encoding `CompareObjects`. `X509_STORE_free` still throws for a store holding certificates, a null store throws, and CRL lookups and names needing UTF-8 conversion throw. Closes when: compared with the console.

## remove `V3JtebYui1A`
