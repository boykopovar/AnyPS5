# libc

## add Functional

- [cacos, cacosh, casin, casinh, catan, catanh, ccos, ccosh, cexp, csin, csinh, csqrt, ctan, ctanh and cpow](../../../core/libs/prx/libc/src/FloatBuiltins.cpp) (libc) - the `double` forms compute through `std::complex<double>` as C99 Annex G defines them, including the branch cuts. The `float` and `long double` forms are not exported yet. The console's results at the branch cuts and for special values are unverified.
- [clog](../../../core/libs/prx/libc/src/ComplexLog.cpp) (libc) - FreeBSD 11.4's [`s_clog.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/s_clog.c) over this library's msun `log`, `log1p`, `hypot` and `atan2`, which keeps the real part near `|z| = 1` by summing the squares exactly. The console's results are unverified.
