# libSceLibreSsl3

## add Functional

- [EC_POINT_get_affine_coordinates_GFp and EC_POINT_set_affine_coordinates_GFp](../../../core/libs/prx/libSceLibreSsl3/src/EcPointAffine.cpp) (libSceLibreSsl3) - nothing is checked on the console. The getter rejects a point at infinity with `EC_R_POINT_AT_INFINITY` and writes a coordinate only when its output is non-null. The setter reduces each coordinate modulo the field prime, stores it, then fails with `EC_R_POINT_IS_NOT_ON_CURVE` when the point is off the curve and leaves the coordinates stored, as LibreSSL 4.0.0 `ec_lib.c` does; a null coordinate queues `ERR_R_PASSED_NULL_PARAMETER`. The cross-curve `EC_R_INCOMPATIBLE_OBJECTS` check is this port's model, because the reference compares method pointers. Coordinates cross the guest boundary through the modelled 24-byte BIGNUM and mbedtls, and the context argument is unused. Closes when: compared with the console.

## remove `+Tu0gXSas9s`

## remove `8AGfaSvdDq4`
