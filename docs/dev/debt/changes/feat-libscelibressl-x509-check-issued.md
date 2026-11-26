# libSceLibreSsl

## add Unknown function info

- [X509_check_issued](../../../core/libs/prx/libSceLibreSsl/src/X509.cpp) (libSceLibreSsl) - nothing is checked on the console. A certificate without `EXFLAG_SET` throws, as `x509v3_cache_extensions` is unimplemented, and so does a modified `X509_NAME` (no re-encoding); names that differ return 29 first. Null or unreadable pointers that LibreSSL dereferences throw. The rest follows LibreSSL 2.5.4 [`v3_purp.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/x509v3/v3_purp.c#L821-L881), [`X509_NAME_cmp`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/x509/x509_cmp.c#L214-L234) and the [`x509_st`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/x509/x509.h#L268-L294) layout. Closes when: extension caching and name re-encoding are implemented and the export is measured on a PS5.
