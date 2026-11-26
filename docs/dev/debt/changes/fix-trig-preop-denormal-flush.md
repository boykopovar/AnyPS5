# shader

## add Functional

- [v_trig_preop_f64](../../../core/shader/recompiler/Translation/src/Float64Instructions.cpp) flushes a denormal result to zero when bit 1 of the `FLOAT_MODE` f16/f64 denormal field is clear (modes 0 and 1), and keeps it in modes 2 and 3; the PS5's gfx1013 is not measured. On RDNA2 (gfx1036) every result matches over 21 inputs (zeros, denormals, normals up to 2^1023, infinities and NaNs) and 10 segment selectors in all four modes at IEEE 0 and 1, the denormal results coming from segment 20 (19 for 2^100).
