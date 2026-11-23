# shader

## add Functional

- [v_mad_legacy_f32 and v_mac_legacy_f32 in directed round modes](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) round the product and then the sum in the `FLOAT_MODE` f32 round mode, declared or set by `s_round_mode`, each once and exactly, with the zero factor rule and the denormal flushes of round to nearest even; a sum of two zeros is -0 when rounding toward negative infinity and either is -0. Measured on RDNA1 (gfx1010, RX 5700 XT, AMD Windows driver 32.0.21045.5002) over 2844 rows per instruction in all four round modes and f32 denormal modes at IEEE 0 and 1, and over 34128 rows with `s_round_mode`: no mismatch; the PS5's gfx1013 is not measured.
