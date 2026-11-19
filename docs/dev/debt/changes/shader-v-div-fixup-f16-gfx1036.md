# shader

## change [v_div_fixup_f16]
- [v_div_fixup_f16](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) flushes its src1 and src2 to a zero of the same sign when bit 0 of the `FLOAT_MODE` f16 denormal field is clear (0 and 2) and keeps a denormal src0 and result in every mode, as `v_div_fixup_f32` does with the f32 field. Measured on RDNA2 (`gfx1031`, Radeon RX 6700 XT, AMD Windows driver 32.0.21045.5002 through HIP, over 6859 triples at `IEEE=0/1` with `denorm16` 0..3 and `FP16_OVFL` 0/1; corroborated on `gfx1036` across `denorm16={0..3}`). Shaders without a known float mode keep the previous lowering. The PS5's gfx1013 is not measured.
