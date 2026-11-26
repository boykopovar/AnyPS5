# libSceLibreSsl

## add Functional

- [SSL_set_bio](../../../core/libs/prx/libSceLibreSsl/src/SslLifecycle.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 2.5.4 `ssl_lib.c`: it detaches a write-buffering BIO still in place, frees the previous chains, freeing a shared read/write BIO once, and stores the new pair. Closes when: compared with the console.
- [BIO_printf](../../../core/libs/prx/libSceLibreSsl/src/Bio.cpp) (libSceLibreSsl) - nothing is checked on the console. Formats through `vsnprintf` into a guest-heap buffer, writes the whole result and returns the byte count, or -1 when formatting, allocation or a short write fails; LibreSSL 2.5.4 `bio_lib.c` formats in a fixed stack buffer and retries, so only the allocation strategy differs. Closes when: compared with the console.
