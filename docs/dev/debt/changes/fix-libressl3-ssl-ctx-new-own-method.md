# libSceLibreSsl3

## change [TLS_method and TLS_server_method]

- [TLS_method and TLS_server_method](../../../core/libs/prx/libSceLibreSsl3/src/Ssl.cpp) - not checked on a console. Both return the same static method with `dtls` 0, `server` 1, version `0x0304` and the `0x0301`-`0x0304` range of [LibreSSL 4.0.0 `ssl_methods.c`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libssl/ssl_methods.c#L246-L264), which `SSL_CTX_set_min_proto_version` and `SSL_CTX_set_max_proto_version` read. The 16-byte `LibreSsl3SslMethod` has none of the upstream handshake callbacks or `enc_flags`, so `SSL_CTX_new` throws for it instead of passing it to libSceLibreSsl, whose 2.5.4 `SSL_CTX_new` would read its `internal` pointer 24 bytes past the object; no context or connection can be built from it yet. Closes when: the method layout is measured on the firmware module and a context constructor uses it.
