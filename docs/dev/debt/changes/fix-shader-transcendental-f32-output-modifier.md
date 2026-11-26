# shader

## add Functional

- [The f32 transcendental output modifier](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) (`mul:2`, `mul:4`, `div:2`) of `v_rcp_f32`, `v_rcp_iflag_f32`, `v_rsq_f32`, `v_sqrt_f32`, `v_log_f32`, `v_exp_f32` and `v_cos_f32` is applied whenever IEEE mode is off, in all four f32 denormal modes, with the bit-level result of the f32 path, and ignored with IEEE mode on. Measured on RDNA2 (gfx1036, AMD Windows driver 32.0.21042.62) with [tools/hw-oracle](../HW_ORACLE.md); the PS5's gfx1013 is not measured. `v_sin_f32` and the f16 forms follow the general rule on the same GPU; `v_rsq_clamp_f32`, absent on gfx1036, is unmeasured and keeps it. Pinned by [TranscendentalOutputModifier.cpp](../../../core/libs/prx/libSceAgcDriver/tests/execution/TranscendentalOutputModifier.cpp).
