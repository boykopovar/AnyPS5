# shader

## change [SDWA unary-f16 byte selectors]

- [SDWA unary-f16 byte selectors](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured, and approximation accuracy past the tested inputs is not checked. `v_rcp_f16`, `v_sqrt_f16`, `v_rsq_f16`, `v_log_f16`, `v_exp_f16`, `v_floor_f16`, `v_ceil_f16`, `v_trunc_f16`, `v_rndne_f16`, `v_fract_f16`, `v_sin_f16` and `v_cos_f16` accept byte sources and destinations. Source selection and zero/sign extension precede the f16 operation; byte destinations place the low eight result bits with pad, sign-extend or preserve. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §§6.8, 12.19.2, 13.3.7 Table 88 and LLVM 19.1.0 gfx10_asm_vop1.s. On RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) `SdwaUnaryF16Bytes`' expected words and NaN classification hold for all 12 cases, wave32 and wave64.
