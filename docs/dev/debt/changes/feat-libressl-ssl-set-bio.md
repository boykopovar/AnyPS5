# libSceLibreSsl

## add Functional

- [SSL_set_bio](../../../core/libs/prx/libSceLibreSsl/src/SslLifecycle.cpp) (libSceLibreSsl) - nothing is checked on the console. It follows `SSL_set_bio` of LibreSSL 2.5.4's [ssl_lib.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_lib.c#L562-L579) through the helper `SSL_set_fd` already uses: it detaches a write buffering `BIO`, frees the old read chain through `BIO_free_all` when the read `BIO` changes and is not also the write `BIO`, and frees the old write chain when the write `BIO` changes. As in that release, replacing only the write side of a connection whose read and write `BIO` are the same frees a `BIO` that stays installed as the read side. A null connection throws where LibreSSL faults.
