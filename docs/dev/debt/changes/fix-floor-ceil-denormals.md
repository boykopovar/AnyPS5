# libc

## add Functional

- [floor, ceil, floorf and ceilf](../../../core/libs/prx/libc/src/Math.cpp) (libc) work on the bits, after FreeBSD msun's `s_floor.c`, `s_ceil.c`, `s_floorf.c` and `s_ceilf.c`, instead of calling the host: with denormals-are-zero set in MXCSR the host treated a subnormal argument as zero, so `ceil` of a positive one returned 0 and `floor` of a negative one -0, where the `libc.prx` bundled with PPSA10122 01.101.000 returns 1 and -1 (0x47dc0, 0x42f70, 0x47f00, 0x430a0). `FE_INEXACT` comes from adding the argument to a large constant, which under denormals-are-zero raises nothing for a subnormal. Results and flags equal that module's on 300,000 inputs per function in the four rounding modes and with DAZ, FTZ and both, run natively on Windows; not measured on a console.
