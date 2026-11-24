# libSceLibreSsl3

## add Functional

- [EC_POINT_get_affine_coordinates_GFp and EC_POINT_set_affine_coordinates_GFp](../../../core/libs/prx/libSceLibreSsl3/src/EcPointAffine.cpp) (libSceLibreSsl3) - nothing is checked on the console. Follows LibreSSL 4.0.0 `ecp_smpl.c` and `ec_lib.c`: the getter rejects a point at infinity with `EC_R_POINT_AT_INFINITY` and writes a coordinate only when its output is non-null; the setter reduces each coordinate modulo the field prime, stores it, then fails with `EC_R_POINT_IS_NOT_ON_CURVE` when the point is off the curve, leaving the stored coordinates in place as LibreSSL does, and a null coordinate queues `ERR_R_PASSED_NULL_PARAMETER`. The cross-curve `EC_R_INCOMPATIBLE_OBJECTS` check in both directions is this port's model, because the group and point carry NIDs where the reference compares method pointers. The coordinates cross the guest boundary through the modelled 24-byte BIGNUM and mbedtls, and the context argument is unused because the temporaries live in host memory. Closes when: compared with the console.

## remove `+Tu0gXSas9s`

## remove `8AGfaSvdDq4`
