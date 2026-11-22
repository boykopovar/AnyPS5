# libSceLibreSsl

## add Unknown function info

- [SSL_get0_param](../../../core/libs/prx/libSceLibreSsl/src/Ssl.cpp) (libSceLibreSsl) - LibreSSL 2.5.4, whose version string the module holds, has no `SSL_get0_param`; it first appears in the [LibreSSL 2.7.0](https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-2.7.0.tar.gz) `ssl_lib.c`, which returns the connection's `param`. This reads that pointer at offset 64 of the 2.5.4 232-byte `SSL` layout the other `SSL` accessors use, which is unverified for a module that exports a later function. A null connection, which LibreSSL dereferences, throws. Closes when: compared with the console.
