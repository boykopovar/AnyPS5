# shader

## add Functional

- [v_div_fmas_f64 in directed round modes](../../../core/shader/recompiler/Translation/src/Float64Instructions.cpp) is not measured on the PS5's gfx1013. It matches RDNA1 (gfx1010, RX 5700 XT, AMD Windows driver 32.0.21045.5002) over 1040 rows in all four round modes, f16/f64 denormal modes 0 and 3 and IEEE 0 and 1, and in modes 1 and 2 set by `s_round_mode`. `src0 * src1 + src2`, scaled by 2^128 or 2^-128 when VCC is set, is rounded once in the f16/f64 round mode by the integer fused multiply-add of `v_fma_f64`: denormal inputs are kept, an overflow gives the largest finite value of its sign when the mode rounds toward zero for that sign, and a sum that cancels exactly is -0 only when rounding down. When bit 1 of the f16/f64 denormal field is clear, a sum below the smallest normal once rounded to 53 bits with an unbounded exponent becomes a zero of its sign.
