# libSceLibreSsl3

## add Functional

- [SSL_CTX_set_ciphersuites](../../../core/libs/prx/libSceLibreSsl3/src/SslCtxCiphersuites.cpp) takes LibreSSL's `const char *` prototype and stores the TLS 1.3 list in `LibreSsl3SslCtx::cipherListTls13` as a `LibreSslStack` of cipher ids `0x03001301`-`0x03001303`, because the port has no `SSL_CIPHER` table to point at; the list is not merged into `cipher_list`. Names match by exact bytes against both spellings LibreSSL accepts, the `TLS_*` name and the `AEAD-*` alias; the two CCM suites are recognised and skipped, as LibreSSL 4.0.0 has no cipher for them. An unknown or empty element fails the whole call as `ssl_parse_ciphersuites` does: it returns 0, queues `SSL_R_NO_CIPHER_MATCH` and keeps the previous list, while the empty string is a successful empty list that replaces it. A null context or a null list returns 0 with no error. A successful list frees the stack it replaces. Console behaviour is unverified. Closes when: compared with the console.

## remove `bpOQdoGE2es`
