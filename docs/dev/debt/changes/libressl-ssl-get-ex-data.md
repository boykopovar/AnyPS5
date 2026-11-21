# libSceLibreSsl

## change [SSL_get_ex_new_index, SSL_CTX_get_ex_new_index, SSL_get_ex_data_X509_STORE_CTX_idx and CRYPTO_cleanup_all_ex_data]

- [SSL_get_ex_new_index, SSL_CTX_get_ex_new_index, SSL_get_ex_data_X509_STORE_CTX_idx and CRYPTO_cleanup_all_ex_data](../../../core/libs/prx/libSceLibreSsl/src/ExData.cpp) (libSceLibreSsl) - unverified on a PS5. `SSL_CTX_new`/`SSL_new` and `SSL_CTX_free`/`SSL_free` call the stored new and free callbacks, and the dup callbacks are never called. Index numbering follows LibreSSL 2.5.4's [ex_data.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/ex_data.c#L310-L406), [ssl_lib.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_lib.c#L2828-L2854) and [ssl_cert.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_cert.c#L133-L160): per class from 0, the verify-callback index allocated once and kept across `CRYPTO_cleanup_all_ex_data`, which restarts every class at 0.

## add Unknown function info

- [SSL_get_ex_data](../../../core/libs/prx/libSceLibreSsl/src/Ssl.cpp) (libSceLibreSsl) - nothing is checked on the console. The index selects an element of the connection's ex-data stack, following LibreSSL 2.5.4 [`ssl_lib.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_lib.c) and [`ex_data.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/ex_data.c#L602-L626): an absent stack, an index at or past the stack length and a negative index return null, and a null connection or null internal state throws where LibreSSL dereferences them.
