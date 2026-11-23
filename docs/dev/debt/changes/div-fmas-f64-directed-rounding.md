# shader

## add Functional

- [v_div_fmas_f64 in directed round modes](../../../core/shader/recompiler/Translation/src/Float64Instructions.cpp) rounds `src0 * src1 + src2`, scaled by 2^128 or 2^-128 when VCC is set, once in the `FLOAT_MODE` f16/f64 round mode, declared or set by `s_round_mode`, through the integer f64 fused multiply-add of `v_fma_f64` with the scale added to the result exponent: denormal inputs are kept, an overflow gives the largest finite value of its sign when the mode rounds toward zero for that sign, an exact zero sum is -0 only when rounding down, and a denormal result is flushed when bit 1 of the f16/f64 denormal field is clear. Measured on RDNA1 (gfx1010, RX 5700 XT, AMD Windows driver 32.0.21045.5002) over 1024 rows in all four round modes with f16/f64 denormal modes 0 and 3 at IEEE 0 and 1, and in modes 1 and 2 declared and set by `s_round_mode`: no mismatch; the PS5's gfx1013 is not measured.
