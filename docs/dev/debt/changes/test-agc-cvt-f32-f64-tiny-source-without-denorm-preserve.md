# shader

## add Functional

- [v_cvt_f32_f64 without DenormPreserve 32](../../../core/shader/recompiler/SpirvBackend/src/SpirvAlu/SpirvAluEmitterFloat64.cpp): on a device whose f32 denormals are not preserved, the driver may flush an f64 source below 2^-126 before rounding. RADV on Renoir (Mesa 26.0.8) gives a zero of the source's sign for `0x380fffffe0000000` in `FLOAT_MODE` 0xe0, where RDNA2 (gfx1031) rounds it up to `0x00800000`. `agc_driver_cvt_f32_f64_denormal_result` leaves such sources out on these devices, as it does expected f32 denormals. Closes when: the conversion of a source below 2^-126 does not depend on the driver's denormal handling.
