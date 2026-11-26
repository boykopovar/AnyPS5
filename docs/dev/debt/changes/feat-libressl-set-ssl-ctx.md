# libSceLibreSsl

## add Functional

- [SSL_set_SSL_CTX](../../../core/libs/prx/libSceLibreSsl/src/SslLifecycle.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 2.5.4 `ssl_lib.c`: a null context selects the connection's initial context, the context already in use is returned unchanged, the new context's certificate is duplicated, the replaced context reference is released and the new one retained, and the context in use is returned. Certificate duplication only covers the modelled empty certificate, because the shared helpers throw for a context carrying a certificate or private key. Closes when: compared with the console.
