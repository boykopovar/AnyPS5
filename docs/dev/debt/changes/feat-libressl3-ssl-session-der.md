# libSceLibreSsl3

## remove `5aSlVeVSKVc`

## remove `IiG4HLr+cOw`

## add Functional

- [d2i_SSL_SESSION and i2d_SSL_SESSION](../../../core/libs/prx/libSceLibreSsl3/src/SslSessionDer.cpp) (libSceLibreSsl3) - nothing is checked on the console. They follow `SSL_SESSION_encode`/`d2i_SSL_SESSION` in [ssl_asn1.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libssl/ssl_asn1.c) for the modelled 192-byte prefix, in its field order and optional forms. `cipher_value` is not modelled, so the required cipher suite is written as `0x0000` and discarded when decoding, and the zero-cipher refusal is not reproduced; a stored length above a modelled array returns -1. A present peer certificate [3] throws on decoding (no `d2i_X509`), and so do [6] hostname, [9] ticket lifetime and [10] ticket (no model field); all three are absent when encoding. `references` and `peer_cert_type` are not encoded: new sessions get references 1, existing ones keep theirs. Closes when: compared with the console.
