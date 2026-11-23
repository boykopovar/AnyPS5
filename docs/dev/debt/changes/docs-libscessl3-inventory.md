# libSceLibreSsl3

## add Functional

- [SSL_CTX_set_max_early_data, SSL_CTX_set_post_handshake_auth, SSL_set_quic_use_legacy_codepoint and SSL_process_quic_post_handshake](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) (libSceLibreSsl3) - nothing is checked on the console. Their bodies follow [LibreSSL 4.0.0 ssl_lib.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libssl/ssl_lib.c): `SSL_CTX_set_max_early_data` and `SSL_process_quic_post_handshake` return 1, and `SSL_CTX_set_post_handshake_auth` and `SSL_set_quic_use_legacy_codepoint` return without action. As in that revision, early data and QUIC are not implemented, so those results do not show that an early-data or QUIC request had any effect. Closes when: compared with the console.
