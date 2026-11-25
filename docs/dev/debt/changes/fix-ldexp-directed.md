# libc

## add Functional

- [ldexp](../../../core/libs/prx/libc/src/Math.cpp) (libc) edits the exponent field instead of calling the host's `scalbn`, as the `libc.prx` bundled with PPSA10122 01.101.000 does (0x4a6e0): an overflow returns an infinity and an underflow a zero in every rounding mode, each through `_Feraise`, so with `ERANGE`; a subnormal result is rounded to nearest, ties to even, in every rounding mode and raises nothing, even when bits are lost. Results, flags and `errno` equal that module's on 300,000 inputs in each of the four rounding modes and with DAZ and FTZ, run natively on Windows; not measured on a console. `ldexpf` is `scalbnf` in that module and follows the rounding mode.
