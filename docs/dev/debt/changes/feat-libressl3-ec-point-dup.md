# libSceLibreSsl3

## add Functional

- [EC_POINT_dup](../../../core/libs/prx/libSceLibreSsl3/src/EcPointDup.cpp) - copies the point with `mbedtls_ecp_copy` into fresh storage of the supplied group, so the copy can outlive the source; a null point returns null, a null group throws, and a point of another curve is unimplemented and throws (LibreSSL copies it). Allocation or copy failure frees the half-built copy and throws. The `EcPoint` wrapper layout and the console behaviour are unverified. Closes when: compared with the console.

## remove `uITUdhDDAg8`
