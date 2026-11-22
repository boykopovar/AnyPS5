# libSceLibreSsl

## add Unknown function info

- [SSL_get_servername](../../../core/libs/prx/libSceLibreSsl/src/Ssl.cpp) (libSceLibreSsl) - nothing is checked on the console. As in [LibreSSL 2.5.4](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libssl/ssl_lib.c) `ssl_lib.c`, a type other than `TLSEXT_NAMETYPE_host_name` (0) returns null, and otherwise the connection's `tlsext_hostname` (offset 184 of `SSL`) is returned unless it is null and a session is set, in which case the session's, at offset 192 of the 232-byte `SSL_SESSION`; both offsets come from compiling 2.5.4's `ssl_locl.h`. A null connection with type 0, which LibreSSL dereferences, throws. Closes when: compared with the console.
