# libSceLibreSsl3

## add Functional

- [SSL_CTX_set_ciphersuites](../../../core/libs/prx/libSceLibreSsl3/src/SslCtxCiphersuites.cpp) takes LibreSSL's `const char *` prototype and stores the TLS 1.3 list in `LibreSsl3SslCtx::cipherListTls13` as a `LibreSslStack` of cipher ids `0x03001301`-`0x03001305`, because the port has no `SSL_CIPHER` table to point at. Names match by exact bytes against both spellings LibreSSL accepts, the `TLS_*` name and the `AEAD-*` alias; an unknown name fails the whole call as `ssl_parse_ciphersuites` does, so a null context, a null list or a list with any unknown or empty element returns 0 and keeps the previous list, while the empty string is a successful empty list that replaces it. A successful list frees the stack it replaces. Console behaviour is unverified. Closes when: compared with the console.

## remove `bpOQdoGE2es`
