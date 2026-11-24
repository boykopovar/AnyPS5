# libSceLibreSsl3

## add Functional

- [EC_POINT_get_affine_coordinates_GFp and EC_POINT_set_affine_coordinates_GFp](../../../core/libs/prx/libSceLibreSsl3/src/EcPointAffine.cpp) (libSceLibreSsl3) - nothing is checked on the console. Follows LibreSSL 4.0.0 `ecp_smpl.c` and `ec_lib.c`: the getter rejects a point at infinity with `EC_R_POINT_AT_INFINITY` and writes a coordinate only when its output is non-null, the setter rejects a null coordinate with `ERR_R_PASSED_NULL_PARAMETER` and, like LibreSSL, does not validate the point against the curve, and both reject a point of another curve with `EC_R_INCOMPATIBLE_OBJECTS`. The coordinates cross the guest boundary through the modelled 24-byte BIGNUM and mbedtls, so a coordinate that does not fit the field makes the setter return 0, and the context argument is unused because the temporaries live in host memory. Closes when: compared with the console.

## remove `+Tu0gXSas9s`

## remove `8AGfaSvdDq4`
