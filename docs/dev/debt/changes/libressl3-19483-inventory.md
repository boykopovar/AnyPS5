# libSceLibreSsl3

## change [libtls CA memory and verification settings]

- [libtls CA memory and verification settings](../../../core/libs/prx/libSceLibreSsl3/src/LibTls.cpp) - Five exports copy owned CA and certificate bytes, assign a protocol bitmask, and set client-certificate and hostname-verification flags, following [OpenBSD libtls `tls_config.c`](https://github.com/openbsd/src/blob/master/lib/libtls/tls_config.c). Server configuration now parses a certificate and key from memory, checks the pair and reports LibreSSL's configure-time errors; the exact strings, console ABI and option interoperability are unverified. Client certificate parsing beyond pairing, non-preset cipher-list validation and console error parity remain unimplemented; `tls_config_parse_protocols` is implemented in `TlsProtocols.cpp`.

## change [libtls context and configuration lifecycle]

- [libtls context, configuration and transport](../../../core/libs/prx/libSceLibreSsl3/src/LibTlsTransport.cpp) - no PS5 ABI, error string or mbedTLS parity is console-verified. Client and accepted server sessions run over guest sockets using config, CA, certificate, key and verification flags from `LibTls.cpp`; the system `CA_LIST.cer` fallback is used only when a client or a server with `tls_config_verify_client` needs trust anchors. The accepted context keeps its own session and the listening context can accept again. TLS 1.3 is disabled in the bundled mbedTLS config, so TLS 1.2 is negotiated; client certificates configured on a client still throw, as do non-preset cipher lists. OCSP, ALPN, SNI, session reuse and renegotiation are absent. Closes when: compared with the console.

## change [tls_config_set_ciphers]

- [tls_config_set_ciphers](../../../core/libs/prx/libSceLibreSsl3/src/LibTls.cpp) - a cipher string other than the libtls presets throws, because validating it needs `SSL_CTX_new` and `SSL_CTX_set_cipher_list`, which throw in libSceLibreSsl; the console accepts or rejects such strings. Signature, names and preset strings follow [LibreSSL 4.0.0 `tls_config.c`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libtls/tls_config.c) and `tls_internal.h`, unverified on a console. A null configuration returns -1 instead of faulting. `tls_config_new` leaves the stored string empty rather than `secure`; `tls_connect_socket` throws when `ciphers` is set instead of silently ignoring it.

## remove `27Fuliv3Om8`

## remove `7RvDjfy4gUA`

## remove `ZERQN8ewpgk`

## remove `fZJr2OF+KGw`
