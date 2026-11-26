# libSceLibreSsl

## add Functional

- [SSL_get0_param, SSL_get_servername and SSL_CTX_set_alpn_protos](../../../core/libs/prx/libSceLibreSsl/src/SslConfig.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 2.5.4 `ssl_lib.c`: `SSL_get0_param` returns the connection's verify parameters, `SSL_get_servername` checks the name type first and returns the connection's client name or NULL, and `SSL_CTX_set_alpn_protos` frees the previous wire-format list, copies the new one and returns 0 on success or 1 for a null or empty list, attachment failure or an unreadable guest list. The name stored in a session is not returned because the modelled session layout does not expose it. Closes when: compared with the console.
