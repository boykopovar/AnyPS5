# libSceLibreSsl

## change [SSL_set_fd and SSL_get_fd]

- [SSL_set_fd and SSL_get_fd](../../../core/libs/prx/libSceLibreSsl/src/SslBio.cpp) (libSceLibreSsl) - nothing is checked on the console. `SSL_set_fd` creates a `BIO_s_socket` `BIO` with `BIO_NOCLOSE` for any integer, as LibreSSL 2.5.4's [ssl_lib.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_lib.c) does (104-byte `BIO`, 80-byte `BIO_METHOD`, measured on a 2.5.4 x86-64 build). It is the method of [BIO_new_socket](../../../core/libs/prx/libSceLibreSsl/src/BioFileSocket.cpp), which has no create callback. Once an engine exists, every later I/O call reads the descriptor back from the `BIO`, so `SSL_set_fd` moves the connection to the new socket; -1, a closed descriptor and one that is no longer a guest socket fail with `SSL_ERROR_SYSCALL` and `EBADF`, where the console reports `ENOTSOCK` for an open non-socket descriptor.

## change [SSL_read, SSL_peek, SSL_write and SSL_shutdown unmodelled cases]

- [SSL_read, SSL_peek, SSL_write and SSL_shutdown unmodelled cases](../../../core/libs/prx/libSceLibreSsl/src/SslIo.cpp) (libSceLibreSsl) - a peer fatal alert, any other engine failure and a call after a failed handshake throw, where [LibreSSL 2.5.4](https://github.com/libressl/openbsd/tree/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl) queues the alert reason. `s3->alert_dispatch` stays 0, the engine holding unsent alerts. The engine reads and writes the guest socket itself, not through the `BIO` callbacks. Nothing is checked on the console.
