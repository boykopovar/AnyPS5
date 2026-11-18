# libSceLibreSsl3

## remove `5aSlVeVSKVc`

## remove `IiG4HLr+cOw`

## add Functional

- [d2i_SSL_SESSION and i2d_SSL_SESSION](../../../core/libs/prx/libSceLibreSsl3/src/SslSessionDer.cpp) (libSceLibreSsl3) - nothing is checked on the console. They follow `SSL_SESSION_encode` and `d2i_SSL_SESSION` in LibreSSL [ssl_asn1.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libssl/ssl_asn1.c) for the modelled 192-byte session prefix, in the template's order and optional forms: ASN.1 version 1, protocol version, session ID, master key, [1] time, [2] timeout, [3] peer certificate, [4] session ID context and [5] verify result. The session's `cipher_value` is not in the modelled prefix, so the required cipher-suite element is written as `0x0000` and discarded when decoding, and the reference's zero-cipher refusal cannot be reproduced. `peer_cert_type` has no element and is left unchanged; `references` is not encoded, so a newly decoded session gets 1 and an existing one keeps its value. A present peer certificate [3], hostname [6], ticket lifetime [9] or ticket [10] throws when decoding, because this module has no `d2i_X509` and the model has no field for the other three; all three are absent when encoding. A stored length above a modelled array returns -1 instead of reading past it. Null handling, the two-call convention and the error reasons follow the neighbouring `i2d_X509` and `d2i_X509_CRL`, where LibreSSL's CBS helpers queue nothing. Closes when: compared with the console.
