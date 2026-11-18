# libSceLibreSsl3

## add Functional

- [SSL_CTX_set_ciphersuites](../../../core/libs/prx/libSceLibreSsl3/src/SslCtxCiphersuites.cpp) takes LibreSSL's `const char *` prototype and stores the TLS 1.3 list in `LibreSsl3SslCtx::cipherListTls13` as a `LibreSslStack` of cipher ids `0x03001301`-`0x03001305`, because the port has no `SSL_CIPHER` table to point at. Names match by exact bytes and unknown names are skipped; a null context, a null list or a list with no recognised name returns 0 and keeps the previous list; a successful list frees the stack it replaces. Console behaviour is unverified. Closes when: compared with the console.

## remove `bpOQdoGE2es`
