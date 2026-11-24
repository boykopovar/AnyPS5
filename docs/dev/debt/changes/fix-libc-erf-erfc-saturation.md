# libc

## add Unknown function info

- [erf, erfc](../../../core/libs/prx/libc/src/Math.cpp) (libc) - saturated results follow the libc of PS5 SDK 5.00 run natively: `erf(x)` for `|x|` of 6 or more returns exactly 1 or -1 in all four rounding modes and raises no flag, and `erfc(x)` for `x` of -6 or less returns exactly 2 in all four modes, raising inexact only above -28 (x87 status clear on the console, set here) and nothing from -28 down. `errno` stays 0. Float variants and the `erfc` positive tail are unchanged.
