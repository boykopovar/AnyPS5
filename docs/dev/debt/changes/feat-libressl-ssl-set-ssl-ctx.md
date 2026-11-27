# libSceLibreSsl

## add Functional

- [SSL_set_SSL_CTX](../../../core/libs/prx/libSceLibreSsl/src/SslLifecycle.cpp) (libSceLibreSsl) - nothing is checked on the console. It follows `SSL_set_SSL_CTX` of LibreSSL 2.5.4's [ssl_lib.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_lib.c#L2751-L2772): the same context returns at once, a null context means the connection's initial context, the connection gets a copy of the new context's certificate holder with the negotiated digests of the old one, and the context reference moves from the old context to the new one. Where LibreSSL dereferences a failed certificate copy, the connection is left without a certificate holder. A null connection throws where LibreSSL faults.
