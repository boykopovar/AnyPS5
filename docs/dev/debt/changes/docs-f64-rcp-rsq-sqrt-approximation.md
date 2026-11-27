# shader

## add Functional

- [v_rcp_f64, v_rsq_f64 and v_sqrt_f64](../../../core/shader/recompiler/Translation/src/VectorInstructions.cpp) (shader recompiler) return the host driver's f64 reciprocal, inverse square root and square root. RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) returns an approximation of about f32 precision. Over 256 normal inputs from 2^-500 to 2^501, 4, 3 and 0 results respectively were correctly rounded, with largest relative errors of 2^-24.7, 2^-24.2 and 2^-25.3, and the low mantissa bits are mostly zero. On the hardware, `v_sqrt_f64(1.0)` is 0x3ff0000004000000 and `v_rcp_f64(3.0)` is 0x3fd5555554000000. Over 32 normal inputs, the translation differed from the hardware on 30 (`v_rcp_f64`), 31 (`v_rsq_f64`) and 32 (`v_sqrt_f64`) of them, on both NVIDIA (610.74) and AMD's Windows driver. The PS5's gfx1013 is not measured.
