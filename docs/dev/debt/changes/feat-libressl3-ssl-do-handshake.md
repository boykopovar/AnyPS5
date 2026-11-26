# libSceLibreSsl3

## add Functional

- [`SSL_do_handshake`](../../../core/libs/prx/libSceLibreSsl3/src/SslDoHandshake.cpp) (libSceLibreSsl3) - console ABI and layout are unverified. Follows [LibreSSL `ssl_lib.c`](https://github.com/libressl/openbsd/blob/078b3d1df8ca2011c1605cd40418c58fa0f5b143/src/lib/libssl/ssl_lib.c#L2485-L2500) over `libSceLibreSsl`'s 232-byte `LibreSslSsl` and 432-byte `LibreSslSslInternal` layouts (matching the parent `SSL_new`, `SSL_set_connect_state`, `SSL_connect` and `SSL_accept` exports forwarded through `ParentExports.cpp`): an unset `handshakeFunc` queues `SSL_R_CONNECTION_TYPE_NOT_SET` (144) in the parent error queue without file or line and returns `-1`; states with `SSL_ST_INIT` or `SSL_ST_BEFORE` dispatch `handshakeFunc`; completed states return `1`. Renegotiation (`renegotiate != 0`), null or unreadable `SSL` and internal pointers throw instead of faulting or running unmodelled state transitions. Closes when: compared with the console.
