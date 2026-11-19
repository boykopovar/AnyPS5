# libc

## add Functional

- [lround, llround, lroundf and llroundf](../../../core/libs/prx/libc/src/Math.cpp) (libc) round as the `libc.prx` bundled with PPSA10122 01.101.000 does (0x4cf60, 0x4cd00, 0x4cf90, 0x4cd30): the largest value below 0.5 is added to the magnitude in the argument's own type and the caller's rounding mode, the sum is truncated and converted. So nearly every non-zero argument raises `FE_INEXACT`, an integral one included, and in the directed modes the value follows the mode (`lround(2.5)` is 2 when rounding down, `lround(0x1p53)` is 2^53 + 2 when rounding up). Results and flags equal that module's on 300,000 inputs per function in the four rounding modes and with DAZ and FTZ, run natively on Windows; not measured on a console. `lroundl` and `llroundl` are not changed here.
