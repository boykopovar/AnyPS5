# libSceLibreSsl3

## remove `mhAgol4ETgY`

## remove `U1dQZ1rXHdA`

## add Functional

- [SSL_provide_quic_data and SSL_get_peer_quic_transport_params](../../../core/libs/prx/libSceLibreSsl3/src/QuicData.cpp) (libSceLibreSsl3) - nothing is checked on the console. `SSL_provide_quic_data` queues the bytes in a growable heap buffer reached through the modelled `tls13` pointer at offset 232 of `LibreSsl3Ssl`, which keeps the 320-byte layout; the level only has to be one of the four `ssl_encryption_level_t` values, since the TLS 1.3 read level LibreSSL compares against is not modelled, and unreadable guest data returns 0 where LibreSSL faults. `SSL_get_peer_quic_transport_params` reports `quicTransportParams` and `quicTransportParamsLength` without copying the bytes. Closes when: compared with the console.
