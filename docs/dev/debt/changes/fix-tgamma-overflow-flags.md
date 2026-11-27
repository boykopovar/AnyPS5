# libc

## change [tgamma, lgammaf, lgamma_r, lgammaf_r, gamma, gammaf, gamma_r and gammaf_r]

- [lgammaf, lgamma_r, lgammaf_r, gamma, gammaf, gamma_r and gammaf_r](../../../core/libs/prx/libc/src/Math.cpp) (libc) are unverified on console. `lgammaf`, `gamma`, `gammaf` set Γ(x) sign in `signgam` (NID `5gOOC0kzW0c`, 0 initially per FreeBSD [`s_signgam.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/s_signgam.c)); `*_r` set it in out-pointer. Wrappers follow FreeBSD 11 [`e_gamma.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/e_gamma.c), [`e_gamma_r.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/e_gamma_r.c), [`e_gammaf.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/e_gammaf.c) and [`e_gammaf_r.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/e_gammaf_r.c).

## add Functional

- [tgamma](../../../core/libs/prx/libc/src/TrueGamma.cpp) (libc) follows the split-precision gamma, logarithm and exponential in [FreeBSD msun](https://github.com/freebsd/freebsd-src/tree/afced35c1d2057e168e91a50768ca4d6535a5974/lib/msun/bsdsrc), with fused binary64 arithmetic, guest exponent scaling and reflection. Result bits and MXCSR flags were compared to the title libc modules of PPSA26344 and PPSA28050 across all four rounding modes, including signed zero, overflow, negative integer poles, underflow and NaN payloads. The full population on other SDKs and physical console behavior remain unverified. [SDK 5.00 comparison](https://github.com/boykopovar/AnyPS5/issues/23411).
