# libc

## add Functional

- [ccos, ccosh, cexp, clog, csin, csinh, csqrt and cpow](../../../core/libs/prx/libc/src/FloatBuiltins.cpp) (libc) - the `double` forms compute through `std::complex<double>` as C99 Annex G defines them, including the branch cuts. The `float` forms are not exported yet. The console's results at the branch cuts and for special values are unverified.
- [ctanh and ctan](../../../core/libs/prx/libc/src/ComplexTanh.cpp) (libc) - FreeBSD 11.4's [`s_ctanh.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/s_ctanh.c) over this library's msun `tan`, `sin`, `cos`, `sinh` and `exp`, sharing the code of `ctanhl` and switching to `±1` from a real part of magnitude 22, as FreeBSD does. The console's results are unverified.
- [cacos, cacosh, casin, casinh, catanh and catan](../../../core/libs/prx/libc/src/ComplexInverseTrig.cpp) (libc) - FreeBSD 11.4's [`catrig.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/catrig.c), sharing the code of the `long double` forms. The console's results are unverified.
