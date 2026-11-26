# shader

## add Functional

- [v_rcp_f32 and v_rcp_iflag_f32 denormal results](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) are flushed to a zero of the result's sign in every `FLOAT_MODE` f32 denormal mode, as RDNA2 (gfx1036, AMD Windows driver 32.0.21042.62, HIP runtime) returns for inputs above 2^126 in magnitude at IEEE=0 and IEEE=1 with f32 denormals flushed (0xc0) and kept (0xf0), also through a negated VOP3 source; the PS5's gfx1013 is not measured. `v_rcp_f16` already matches the hardware in both f16 denormal modes. `v_rcp_f64` still returns a denormal result with f64 denormals flushed, where the same GPU returns a zero of its sign.
