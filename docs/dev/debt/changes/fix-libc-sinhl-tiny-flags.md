# libc

## change [sinhl]

- [sinhl](../../../core/libs/prx/libc/src/Math.cpp) (libc) - returns the input unchanged for |x| below 2^-32 without raising `FE_INEXACT`, where FreeBSD msun's [`e_sinhl.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/ld80/e_sinhl.c) raises it on purpose; the SDK 5.00 `libc.prx` module raises no flag there ([#22838](https://github.com/boykopovar/AnyPS5/issues/22838)). Other ranges and other SDK builds are not verified.
