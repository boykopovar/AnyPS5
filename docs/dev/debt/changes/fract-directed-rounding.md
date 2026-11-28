# shader

## add Functional

- [v_fract_f32, v_fract_f16 and v_fract_f64 in directed round modes](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) round x - floor(x) once in the `FLOAT_MODE` round field of their precision, f32 for `v_fract_f32` and f16/f64 for the others, then clamp below 1. Only a negative input with a fractional part below 0.5 is inexact, so toward zero acts as toward -infinity. Measured on RDNA1 (gfx1010, RX 5700 XT, AMD Windows driver 32.0.21045.5002) over 4824 f32, 3222 f16 and 15924 f64 inputs in all four round modes with denormals kept and flushed at IEEE 0 and 1, and with `s_round_mode`; the PS5's gfx1013 is not measured.
