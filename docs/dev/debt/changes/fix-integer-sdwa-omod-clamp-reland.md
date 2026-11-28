# shader

## add Functional

- [SDWA clamp and omod on integer opcodes](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured, nor omod on the VOP1 opcodes with an f32 source and an integer result (`v_cvt_i32_f32`, `v_frexp_exp_i32_f32` and others), which still throws. SDWA omod on integer VOP1 and VOP2 opcodes is dropped, as is clamp on `v_mov_b32`, `v_not_b32`, `v_bfrev_b32`, the bit scans, `v_cndmask_b32`, min/max, the logic ops, the shifts and `v_mul_hi_*24`. On `v_add/sub/subrev_nc_u32`, the `co_ci` results and `v_mul_u32_u24` clamp saturates unsigned and on `v_mul_i32_i24` signed, to 32 bits before the SDWA destination write; the `co_ci` carry-out is not clamped. Measured on RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010) through the [hardware oracle](../HW_ORACLE.md) in wave32.
