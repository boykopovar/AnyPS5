# libc

## add Functional

- [lroundl and llroundl](../../../core/libs/prx/libc/src/Math.cpp) (libc) round with this library's `roundl` and convert, instead of calling the host: an argument too large for the result raises `FE_INVALID` and, for the larger ones, `FE_INEXACT` from `roundl`'s addition, as the `libc.prx` bundled with PPSA10122 01.101.000 does (0x4cfc0, 0x4cd60). Results and flags equal that module's on 300,000 inputs per function at 64- and 53-bit x87 precision and in the three directed rounding modes, run natively on Windows; not measured on a console. At 24-bit precision the module raises `FE_INEXACT` for a zero argument and this does not.
