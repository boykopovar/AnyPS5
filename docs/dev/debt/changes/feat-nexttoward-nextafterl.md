# libc

## add Functional

- [nextafterl, nexttowardl, nexttoward and nexttowardf](../../../core/libs/prx/libc/src/Math.cpp) (libc) follow FreeBSD msun's `s_nextafterl.c`, `s_nexttoward.c` and `s_nexttowardf.c`, and raise overflow and a subnormal result through `_Feraise`, so `errno` becomes `ERANGE` there, as the `libc.prx` bundled with PPSA10122 01.101.000 does (0x48920, 0x4d6b0, 0x4d820). `nexttoward` keeps msun's step in the wrong direction for a positive `double` below 2^-1042 (`nexttoward(0x1p-1074, 3)` is 0); `nexttowardf` does not have it. Result bits, masked flags and `errno` equal that module's on 300,000 inputs per function in six x87 modes and with DAZ, FTZ and the SSE rounding modes, run natively on Windows; not measured on a console, with unmasked exceptions or against another title.
