# shader

## change [`v_cvt_f32_f64` f32 result]
- The [`v_cvt_f32_f64` f32 result](../../../core/shader/recompiler/Translation/src/Float64Instructions.cpp) is rounded to nearest even whatever the `FLOAT_MODE` f32 round mode: with rounding 1 to 3 it differs from RDNA2 (gfx1031) on 5 to 11 of 32 f64 inputs, for example `0xb80fffffffffffff` rounded up gives `0x80800000` where RDNA2 (`gfx1031` and `gfx1036`, measured across all 16 `(round32, round16)` pairs where `round32` governs and `round16` is ignored) gives `0x807fffff`. Its flush to a zero of the source's sign, when bit 1 of the f32 denormal field is clear and the source rounded to 24 bits with an unbounded exponent in the round mode is below 2^-126, is measured on gfx1031 only; the PS5's gfx1013 is not measured.
