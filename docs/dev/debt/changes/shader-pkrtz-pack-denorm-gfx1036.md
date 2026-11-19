# shader

## change [v_cvt_pkrtz_f16_f32 and v_pack_b32_f16 denormals]
- [v_cvt_pkrtz_f16_f32 and v_pack_b32_f16 denormals](../../../core/shader/recompiler/Translation/src/ConvertInstructions.cpp) follow the `FLOAT_MODE` f16 denormal field: `v_cvt_pkrtz_f16_f32` flushes each denormal half result to its signed zero when bit 1 is clear (0 and 1), and `v_pack_b32_f16` flushes each denormal half input to its signed zero when bit 0 is clear (0 and 2). Measured on RDNA2 (`gfx1031`, Radeon RX 6700 XT, AMD Windows driver 32.0.21045.5002 through HIP, at `IEEE=0/1` with `denorm16` 0..3 over 256 pairs each; corroborated on `gfx1036` across `denorm32={0,3}` and `denorm16={0..3}`). Shaders without a known float mode keep the previous lowering. The PS5's `gfx1013` is not measured.
