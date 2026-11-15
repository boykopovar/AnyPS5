# libSceLibreSsl3

## add Functional

- [SSL_get0_verified_chain and SSL_get_peer_quic_transport_params](../../../core/libs/prx/libSceLibreSsl3/src/SslHandshakeState.cpp) (libSceLibreSsl3) - nothing is checked on the console, and no SSL constructor or handshake here fills either field. They read `hs.verified_chain` at offset 520 and `peer_quic_transport_params` with its length at 1000 and 1008 of the 1016-byte `SSL3_STATE` of [LibreSSL 4.0.0 ssl_local.h](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libssl/ssl_local.h#L1091-L1171), as [ssl_lib.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libssl/ssl_lib.c#L945-L952) does; the offsets come from compiling its definitions for LP64, not from the firmware module. A null SSL, and for the transport parameters a null SSL3 state or output, which upstream dereferences, throw before an output is written. Closes when: the layout is measured on the firmware module.

## change [Additional exports]

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 54 still-unimplemented callable exports: 50 named and 4 known only by NID. These entries throw. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. The fourteen libtls lifecycle/config exports are implemented in `LibTls.cpp`; title compatibility beyond preconnection behavior remains unverified.

## remove QKIOxHfZLh8

## remove U1dQZ1rXHdA
