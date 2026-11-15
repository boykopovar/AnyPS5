# libSceLibreSsl3

## change [Additional exports]

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 49 still-unimplemented callable exports: 47 named and 2 known only by NID. These entries throw. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. The fourteen libtls lifecycle/config exports are implemented in `LibTls.cpp` and four client transport exports (`tls_connect_socket`, `tls_handshake`, `tls_read`, `tls_write`) in `LibTlsTransport.cpp`; title compatibility beyond tested client transport remains unverified.

## change `4pEl-5DmrrM`

| `ERR_print_errors_fp` | `4pEl-5DmrrM` |

## change `qGsc6uT6p1E`

| `ECDSA_do_verify` | `qGsc6uT6p1E` |

## change [libtls CA memory and verification settings]

- [libtls CA memory and verification settings](../../../core/libs/prx/libSceLibreSsl3/src/LibTls.cpp) - Five additional exports copy owned CA and certificate bytes, assign a protocol bitmask, and set client-certificate and hostname-verification flags. They follow [OpenBSD libtls `tls_config.c`](https://github.com/openbsd/src/blob/master/lib/libtls/tls_config.c). `tls_config_set_cert_mem` copies the bytes but does not derive the public key hash or parse the certificate as [tls_keypair.c](https://github.com/openbsd/src/blob/master/lib/libtls/tls_keypair.c) does, so malformed certificates are accepted and the error string stays null. Server transport (`tls_accept_socket`), client certificate parsing and cipher-list validation remain unimplemented; `tls_config_set_ciphers` still throws; `tls_config_parse_protocols` is implemented in `TlsProtocols.cpp`. Native PS5 ABI and option interoperability are unverified.

## change [libtls context and configuration lifecycle]

- [libtls context, configuration and client transport](../../../core/libs/prx/libSceLibreSsl3/src/LibTlsTransport.cpp) - Client transport (`tls_connect_socket`, `tls_handshake`, `tls_read`, `tls_write`, `tls_close`) runs over guest sockets via `mbedtls` using config/CA/hostname flags from `LibTls.cpp`, falling back to `CA_LIST.cer` when no CA memory is set. No PS5 ABI, error strings or `mbedtls` parity are console-verified. TLS 1.3 is disabled because `MbedConfig.hpp` undefines `MBEDTLS_SSL_PROTO_TLS1_3` (TLS 1.3-only config fails; default negotiates TLS 1.2), and `tls_accept_socket`, client certs/keys, cipher-list checks (`tls_config_set_ciphers`) and OCSP stapling remain unimplemented.

## remove `27Fuliv3Om8`

## remove `7RvDjfy4gUA`

## remove `ZERQN8ewpgk`

## remove `fZJr2OF+KGw`
