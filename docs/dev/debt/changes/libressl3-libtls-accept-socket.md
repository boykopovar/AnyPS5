# libSceLibreSsl3

## remove `4Q3bvOy4LY8`

## add Functional

- [tls_accept_socket](../../../core/libs/prx/libSceLibreSsl3/src/LibTlsTransport.cpp) (libSceLibreSsl3) - nothing is checked on the console. It follows `tls_accept_fds` and `tls_server_conn` in LibreSSL 4.0.0 [tls_server.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libtls/tls_server.c): a non-server context fails with "not a server context", a failed accept nulls the output, and the accepted context shares the listening context's retained config but owns its session. Errors are set on the listening context. Handshake and I/O run on a host mbedTLS engine over the guest descriptor, so wire bytes, TLS 1.3, SNI, ALPN, OCSP, session tickets and client-certificate requests differ or are absent. A null output pointer, negative descriptor and invalid guest descriptor fail instead of faulting. Closes when: measured on a console.
