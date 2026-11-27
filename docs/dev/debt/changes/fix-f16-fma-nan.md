# shader

## add Functional

- [Scalar f16 fma NaN results](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) (`v_fma_f16`, `v_fmac_f16`, `v_fmamk_f16`, `v_fmaak_f16`) - the PS5's gfx1013 is not measured. A NaN result is the first NaN of source 0, 1 then 2 after their `neg`/`abs` modifiers, with source 2 skipped when the product is infinity times zero (on the flushed operands when FLOAT_MODE flushes f16 input denormals, so infinity times a denormal counts), quieted only with IEEE_MODE=1, or 0xfe00, then the output modifiers, as for the packed instructions; measured on RDNA2 (gfx1036) and pinned in `agc_driver_f16_fma_nan_propagation`. A device whose NaNs the startup probe accepts keeps its own on the native path; the probe now compares the scalar fma results bit for bit, NaN payloads included.
