# libc

## change [tgamma, lgammaf, lgamma_r, lgammaf_r, gamma, gammaf, gamma_r and gammaf_r]

- [lgammaf, lgamma_r, lgammaf_r, gamma, gammaf, gamma_r and gammaf_r](../../../core/libs/prx/libc/src/Math.cpp) (libc) are unverified on console. `lgammaf`, `gamma`, `gammaf` set Γ(x) sign in `signgam` (NID `5gOOC0kzW0c`, 0 initially per FreeBSD [`s_signgam.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/s_signgam.c)); `*_r` set it in out-pointer. Wrappers follow FreeBSD 11 [`e_gamma.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/e_gamma.c), [`e_gamma_r.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/e_gamma_r.c), [`e_gammaf.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/e_gammaf.c) and [`e_gammaf_r.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/msun/src/e_gammaf_r.c).

## add Functional

- [tgamma](../../../core/libs/prx/libc/src/Math.cpp) (libc) uses the host gamma result except for positive inputs above 171.63, where FreeBSD msun [b_tgamma.c](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/bsdsrc/b_tgamma.c) divides by zero. The flag pattern (`FE_DIVBYZERO`, no `FE_OVERFLOW`) matches the PPSA10122 SDK 5.00 reading in [#23411](https://github.com/boykopovar/AnyPS5/issues/23411). Other titles and the exact boundary are unverified. Finite negative integer poles set `EDOM` and return the host NaN (unmeasured). An underflowed zero takes the sign of Γ(x), as in `tgammaf`.
