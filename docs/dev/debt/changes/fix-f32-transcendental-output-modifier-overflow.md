# shader

## add Functional

- The [f32 transcendental output modifier overflow](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) is not measured on the PS5's gfx1013. On RDNA2 (gfx1031, RX 6750 XT, [tools/hw-oracle](../HW_ORACLE.md)), when `mul:2` or `mul:4` takes a finite result of `v_rcp_f32`, `v_rcp_iflag_f32` or `v_exp_f32` past the largest finite value, the result is infinity in every f32 round mode and every f32 denormal mode, where the other f32 results follow the round field. `v_rsq_f32`, `v_sqrt_f32`, `v_log_f32` and `v_cos_f32` share the path and have no result large enough to overflow. `v_rsq_clamp_f32` keeps the round field rule and is not measured.
