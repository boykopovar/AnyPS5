# shader

## add Functional

- [VOP3 32-bit integer ternary clamp](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) (`v_min3_i32`, `v_min3_u32`, `v_max3_i32`, `v_max3_u32`, `v_med3_i32`, `v_med3_u32`) (shader recompiler) - the decoder accepts the VOP3 clamp modifier bit on 32-bit integer min3, max3 and med3 without effect; min, max and med select from input operands and cannot saturate. Checked by [Vop3Integer32Clamp](../../../core/libs/prx/libSceAgcDriver/tests/execution/Vop3Integer32Clamp.cpp); console comparison on gfx1013 remains unmeasured.
