# shader

## add Functional

- [16-bit integer DPP16 and SDWA source modifiers](../../../core/shader/recompiler/Translation/src/TranslationContext/SubwordPacking.cpp) - the PS5's gfx1013 is not measured. VOP3 `v_ldexp_f16` with `abs` or `neg` on src1, the i16 exponent, throws (`VOP3 source modifiers are not implemented`). On `v_cvt_f16_u16`, `v_cvt_f16_i16`, the VOP2 `v_ldexp_f16` src1 and the VOPC SDWA 16-bit integer compares (measured on `v_cmp_lt_i16` and `v_cmp_lt_u16`), `neg` and `abs` change bit 15 of the operand after the DPP lane move and after the SDWA select and extension: `neg` flips it, `abs` clears it, both set it. Measured on RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010) through the [hardware oracle](../HW_ORACLE.md) in wave32.
