# libSceLibreSsl

## change [SSL_set_fd and SSL_get_fd]

- [SSL_set_fd and SSL_get_fd](../../../core/libs/prx/libSceLibreSsl/src/SslBio.cpp) (libSceLibreSsl) - nothing is checked on the console. `SSL_set_fd` creates the socket `BIO` of LibreSSL 2.5.4's [bss_sock.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/bio/bss_sock.c) with `BIO_NOCLOSE` and accepts any integer; the 104-byte `BIO` and 80-byte `BIO_METHOD` match a 2.5.4 x86-64 build. `SSL_free` and `SSL_set_fd` release it via `BIO_free`. Destroy follows `sock_free` and throws when closing the fd; control handles `GET_CLOSE`/`SET_CLOSE`/`FLUSH`/`DUP`, returns 0 on unknown commands and throws on `SET_FD`/`GET_FD`; `read`/`write`/`puts`/`create` throw. Later I/O reads the fd from `BIO`; -1, closed fds and non-sockets fail with `SSL_ERROR_SYSCALL` and `EBADF` (console reports `ENOTSOCK`).
