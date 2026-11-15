# libSceLibreSsl3

## change [Additional exports]

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 51 still-unimplemented callable exports: 49 named and 2 known only by NID. These entries throw. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. The fourteen libtls lifecycle/config exports are implemented in `LibTls.cpp` and four client transport exports (`tls_connect_socket`, `tls_handshake`, `tls_read`, `tls_write`) in `LibTlsTransport.cpp`; title compatibility beyond tested client transport remains unverified.

## change `4pEl-5DmrrM`

| `ERR_print_errors_fp` | `4pEl-5DmrrM` |

## change `qGsc6uT6p1E`

| `ECDSA_do_verify` | `qGsc6uT6p1E` |
