# libc

## change [nearbyintf]

- [nearbyintf](../../../core/libs/prx/libc/src/MathExact.cpp) (libc, NID `c+4r-T-tEIc`) - a PS5 payload process (firmware 11.60) raised no `FE_INEXACT` for the exact inputs 1, -3, 2^23 and 3e38, as here; a title process was not measured. With MXCSR 0x1f80 (DAZ and FTZ off) a payload process on firmware 13.590.001 returned 0, 0, 1.0 and 0 for the subnormal input `0x00000001` under nearest, down, up and toward zero, as the macOS build here does, and raised `FE_INEXACT`; with the payload's own DAZ and FTZ setting (MXCSR 0x9fc0) that input was not measured, and negative subnormals were not measured. A PS5 firmware 13.60 payload measured MXCSR-controlled rounding and `FE_INEXACT` for inexact inputs; this implementation uses host `rintf` for those measured cases.

## change [nearbyint]

- [nearbyint](../../../core/libs/prx/libc/src/MathExact.cpp) (libc) - a PS5 payload process (firmware 11.60) raised no `FE_INEXACT` for the exact inputs 1, -3, 2^52 and 1e300, as here; a title process was not measured. With MXCSR 0x1f80 (DAZ and FTZ off) a payload process on firmware 13.590.001 returned 0, 0, 1.0 and 0 for the subnormal input `0x0000000000000001` under nearest, down, up and toward zero, as the macOS build here does, and raised `FE_INEXACT`; with the payload's own DAZ and FTZ setting (MXCSR 0x9fc0) that input was not measured, and negative subnormals were not measured. The bundled libc of PPSA07923 and PPSA05916 raises `FE_INEXACT` for inexact inputs, which this implementation reproduces with host `rint`.
