# libc

## change [strtod](../../../core/libs/prx/libc/src/Strings.cpp) (libc) sets

- [strtod](../../../core/libs/prx/libc/src/Strings.cpp) (libc) sets `ERANGE` for tested decimal inputs with a nonzero subnormal result, including the exact decimal expansion of 2^-1074, matching nine distinct title-bundled `libc.prx` builds measured on Windows. Two other `libc.prx` builds and the console's `libSceLibcInternal.sprx` leave `errno` alone for decimal subnormals. `wcstod` parses through `strtod` and follows the same rule, as the `wcstod` of the `libc.prx` bundled with eleven titles does (PPSA04158 to PPSA35169, measured on Windows); the console's `libSceLibcInternal` `wcstod` is unmeasured. Hexadecimal subnormal `strtod` and `wcstod` match one title's `libc.prx` only (PPSA08329, SHA-256 a485b34ef160, run on Linux, not on a console). Hexadecimal results below 2^-1074 ignore the rounding mode (`0x1p-1076` rounded upward gives 0, FreeBSD's gdtoa 2^-1074); no `libc.prx` or console result under a directed rounding mode is measured.


## change [_Stodx, _Stofx and _Stoldx unscaled hexadecimal conversion]

- [_Stodx, _Stofx and _Stoldx unscaled hexadecimal conversion](../../../core/libs/prx/libc/src/DinkumwareInternals.cpp) (libc) unmasked exception delivery and non-C locale prefix classification remain unverified. The coefficient expansion and masked floating flags are compared against title-bundled SDK modules under 24-bit, 53-bit and 64-bit x87 precision, not exhaustively over all input strings or on a console. A subnormal result can end one unit from `strtod`'s, `strtof`'s or `strtold`'s: `_Stodx("0x3ffffffffffff5p-1077", 0)` gives `0x0007fffffffffffe`, `strtod` and PPSA08329's `libc.prx` `strtod` `0x0007ffffffffffff`; the modules' own `_Stodx` result for it is unmeasured.
