# libSceLibreSsl

## add Functional

- [SSL_CTX_load_verify_mem](../../../core/libs/prx/libSceLibreSsl/src/SslVerifyMem.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 2.5.4 `x509_d2.c`: each call attaches a copy of the buffer to a memory lookup on the context's store, adding the lookup on the first call and reusing it afterwards, and the copies are released with the store. The lookup does not parse the PEM certificates, because the parent's lookups and certificate decoder are not implemented, so it only queues the bytes for a future verification; a negative length, a null buffer with a length, or a context without a store fails. Closes when: compared with the console.
