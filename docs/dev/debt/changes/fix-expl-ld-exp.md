# libc

## change [expl, sinhl, coshl and tanhl]

- [sinhl, coshl and tanhl](../../../core/libs/prx/libc/src/Math.cpp) (libc) follow FreeBSD msun's ld80 `e_sinhl.c`, `e_coshl.c` and `s_tanhl.c` over a port of [`s_expl.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/s_expl.c) with `k_expl.h` (release/11.4.0), and match that source built on the host bit for bit (30 million inputs at 64-bit and 5 million at 53-bit precision), so they no longer depend on the host's math library. Unverified against the console's `libc.prx`: the 11.4.0 source is the only evidence. msun never touches `errno` (glibc sets `ERANGE`); the port keeps that. They run at the caller's x87 precision. sinhl returns x unchanged for |x| < 2^-32 without raising `FE_INEXACT`, as the SDK 5.00 `libc.prx` module does ([#22838](https://github.com/boykopovar/AnyPS5/issues/22838)), where msun's `e_sinhl.c` raises it on purpose; other ranges and other SDK builds are not verified.

## change [expm1l]

- [expm1l](../../../core/libs/prx/libc/src/MathExact.cpp) (libc) follows the `libc.prx` bundled with PPSA10122 01.101.000 (0x474c0): an argument within ±2^-66 is returned, below -ln 2 and from ln 5 up the result is `expl(x) - 1`, and between those a rational approximation is evaluated at `x`, `x - ln 2` or `x - 2 ln 2`. Overflow, and the underflow of the inner `expl` for a large negative argument, set `errno` to `ERANGE` as that code does. 500,000 arguments, a fifth of them raw 80-bit patterns, give the same bits, exception flags and `errno` as that code run natively on Windows at 64-, 53- and 24-bit precision and at 64-bit precision in each rounding direction. Other SDK versions and the system `libSceLibcInternal` are not compared

## add Functional

- [expl](../../../core/libs/prx/libc/src/MathExact.cpp) (libc) is the wrapper the `libc.prx` bundled with PPSA10122 01.101.000 has at 0x47430: a zero gives 1, an infinity gives itself or `+0`, a NaN is returned unchanged, and any other argument goes through `_LExp` with a factor of 1 and no scaling, which sets `errno` to `ERANGE` on overflow and underflow. 300,000 arguments give the same bits, exception flags and `errno` as that code run natively on Windows at 64-, 53- and 24-bit precision, and at 64-bit precision in each rounding direction. An argument with a nonzero exponent and a clear integer bit, which the x87 never produces, raises only `FE_INVALID` at 53- and 24-bit precision where that code also raises `FE_INEXACT`. Other SDK versions and the system `libSceLibcInternal` are not compared
