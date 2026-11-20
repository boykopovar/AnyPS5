# libSceLibreSsl3

## remove `5aSlVeVSKVc`

## remove `IiG4HLr+cOw`

## add Functional

- [d2i_SSL_SESSION and i2d_SSL_SESSION](../../../core/libs/prx/libSceLibreSsl3/src/SslSessionDer.cpp) (libSceLibreSsl3) - no console check. They follow `SSL_SESSION_encode`/`d2i_SSL_SESSION` in [ssl_asn1.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libssl/ssl_asn1.c) for the modelled 192-byte prefix, in its field order and optional forms. `cipher_value` is not modelled, so the required cipher suite is written as `0x0000` and discarded: LibreSSL accepts the bytes but resumes with `cipher == NULL` and id `0x03000000`, failing the resumed handshake's cipher check. A stored length above a modelled array returns -1. A present [3] peer certificate throws on decoding (no `d2i_X509`), as do [6], [9] and [10] (no model field); those last three are absent when encoding. A non-null peer certificate replaced on decode is dropped unfreed, `X509_free` still being a stub. [2] zero decodes as the default timeout. Closes when: compared with the console.
