# shader

## change [VOP3 16-bit integer binary clamp]

- [VOP3 16-bit integer binary clamp](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) (`v_mul_lo_u16`, `v_lshrrev_b16`, `v_ashrrev_i16`, `v_max_u16`, `v_max_i16`, `v_min_u16`, `v_min_i16`, `v_lshlrev_b16`) (shader recompiler) - the decoder accepts the VOP3 clamp modifier bit on non-packed 16-bit integer multiply, shifts and min/max without effect. On RDNA2 (gfx1036) the bit changes none of the eight 32-bit results: in wave32 over the test's 32 rows plus 32 overflowing products (AMD Windows driver 31.0.22044.1), `v_mul_lo_u16` keeps the low 16 bits of an overflowing product instead of saturating while `v_add_nc_u16` with clamp saturates; in wave64 over the test's 32 rows (driver 32.0.21045.16010) every result matches `Expected`. Checked by [Vop3Integer16Clamp](../../../core/libs/prx/libSceAgcDriver/tests/execution/Vop3Integer16Clamp.cpp); console comparison on gfx1013 remains unmeasured.
