# libc

## add Functional

- [modfl](../../../core/libs/prx/libc/src/MathExact.cpp) (libc) follows FreeBSD msun's `s_modfl.c` instead of the host's `modf`: the fraction of an integral value, an infinity included, is a zero with the sign of `x` (`modfl(-3)` returns -0), and no exception is raised, where the Windows host returned +0 and raised `FE_INEXACT`. Both results and the masked flags equal those of the `libc.prx` bundled with PPSA10122 01.101.000 (0x4d200) on 300,000 inputs in each of six x87 modes, zeros, infinities, NaNs and pseudo-denormals among them, run natively on Windows; not measured on a console.
