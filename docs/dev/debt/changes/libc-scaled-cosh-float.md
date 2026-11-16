# libc

## change [_FSinh and _FCosh]

- [_FSinh](../../../core/libs/prx/libc/src/Math.cpp) (libc) - titles whose `libc.prx` contracts multiply-adds to FMA, such as PPSA13934, differ by one unit in the last place on about 3% of 600,000 sampled calls. The non-contracted sequence matches the `libc.prx` of PPSA07923 and PPSA05916 on those calls, run on a Windows host. NaN preference and payloads, signed zeros, infinite operands and range-error thresholds follow PRAGMATA's bundled `libc.prx`. The system `libSceLibcInternal` and nondefault flush-to-zero modes are unverified; a PS5 firmware 13.60 measurement found the earlier host-based `_FSinh(0.5, y)` one unit in the last place away for y of 0.5, 1 and 2.

## add Functional

- [_FCosh](../../../core/libs/prx/libc/src/Math.cpp) (libc) - the full input space, FMA-contracted SDK builds and nondefault flush-to-zero modes are unverified on a console. Finite results use the binary32 scaled-exponential identity with the existing `_FExp`, the arithmetic of [Dinkumware's scaled cosh](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/xfcosh.c), with the 8.675f boundary read from PRAGMATA's module in [#8023](https://github.com/boykopovar/AnyPS5/pull/8023). The 36 published [PS5 firmware 13.60 payload results](https://gist.githubusercontent.com/orezraey/b230fdbb391a606b8ad8a9427276f40e/raw/eea7b2cd65484c3b4cd016a00f5e413a55424772/libc2.console.log) match locally; those results do not cover subnormal multipliers or floating-point exceptions.
