# shader

## change [v_cvt_pkrtz_f16_f32 and v_pack_b32_f16 denormals]
- [v_cvt_pkrtz_f16_f32 and v_pack_b32_f16 denormals](../../../core/shader/recompiler/Translation/src/ConvertInstructions.cpp) follow the `FLOAT_MODE` f16 denormal field: `v_cvt_pkrtz_f16_f32` flushes subnormal f16 outputs when bit 1 is clear (modes 0 and 1) and keeps them when bit 1 is set (modes 2 and 3), while `v_pack_b32_f16` leaves f16 denormals unchanged in every mode. Measured on RDNA2 (`gfx1031`, RX 6700 XT, AMD Windows driver 32.0.21045.5002, HIP, over 14 f32 and 14 f16 pairs across all 16 `(denorm32, denorm16)` pairs at `IEEE=0/1`; corroborated on `gfx1036` across `denorm32={0,3}` and `denorm16={0..3}`); the PS5's `gfx1013` is not measured.
