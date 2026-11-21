# libSceLibreSsl3

## add Functional

- [X509_STORE_load_locations](../../../core/libs/prx/libSceLibreSsl3/src/X509StoreLoadLocations.cpp) (libSceLibreSsl3) - no console check. Follows the pinned 078b3d1d `x509_d2.c`, `x509_lu.c`, `by_file.c`, `by_dir.c`, `x_name.c` and `a_mbstr.c` (T61String read as Latin-1, unmeasured): a `file` PEM bundle is read and every `CERTIFICATE`, `X509 CERTIFICATE` or `TRUSTED CERTIFICATE` record decoded and added, other records skipped; `dir` is recorded on a hash-dir lookup whose by-subject control loads `<dir>/<8-hex name hash>.<n>` lazily. Gaps: `X509_STORE_free` throws with certificates present, a CRL-only bundle returns 0 where the reference returns 1, a decode failure keeps the certificates added before it, `SEQUENCE` algorithm parameters throw, the `by_subject` object is borrowed, and CRL lookups and a hash-dir lookup name failing canonicalization throw. Closes when: compared with the console.

## remove `V3JtebYui1A`
