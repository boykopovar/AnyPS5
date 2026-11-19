# libc

## change [fmal]

- [fmal](../../../core/libs/prx/libc/src/MathExact.cpp) (libc) is FreeBSD msun's [`s_fmal.c`](https://github.com/freebsd/freebsd-src/blob/release/9.0.0/lib/msun/src/s_fmal.c) of release/9.0.0, the revision before the 2011 rewrite, as the `libc.prx` bundled with PPSA10122 01.101.000 has it (0x48290): with `z` more than 128 binary places below the product, round-to-nearest returns `x * y`, so an exact tie ignores a tiny `z`, as 4f608a8c49 reported for PPSA08427. Its steps go through this library's `nextafterl` and `scalbnl`, so a subnormal step in a directed mode stores `ERANGE` and raises underflow as in the module. At 64-bit x87 precision the bits, the flags and `errno` equal that module's in all four rounding modes, run natively on Windows; not measured on a console. At 53- and 24-bit precision about 4 in 100,000 results, all subnormal, differ.
