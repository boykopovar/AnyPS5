# libc

## change [powl]

- [ldexpl, truncl, logb and remainder](../../../core/libs/prx/libc/src/Math.cpp) (libc) - `ldexpl` is `scalbnl`, one address (0x12630) in the `libc.prx` bundled with PPSA10122 01.101.000, and like that module leaves `errno` alone when the result overflows or underflows; `truncl` is an exact operation and gives the console's result; `logb` and `remainder` give the console's value too, but on Linux glibc sets `errno` (`ERANGE` for `logb(0)`, `EDOM` for `remainder` by zero or of an infinity) where msun only raises the floating-point exception

## add Functional

- [powl](../../../core/libs/prx/libc/src/MathExact.cpp) (libc) follows the `libc.prx` bundled with PPSA10122 01.101.000 (0x4c950 over the routine at 0x4bf70): a base-2 logarithm from a 257-entry table of log2(n/512) plus a polynomial, its product with the exponent kept in 32-bit pieces when it is large, and `_LExp` for the result. The table is computed at first use from its rule, the logarithm cut after 31, 63 and 95 fraction bits, and equals the module's entries. 500,000 pairs give the same bits, exception flags and `errno` as that code run natively on Windows at 64-, 53- and 24-bit precision and at 64-bit precision in each rounding direction. Other SDK versions and the system `libSceLibcInternal` are not compared
