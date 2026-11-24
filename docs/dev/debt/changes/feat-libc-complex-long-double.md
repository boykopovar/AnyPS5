# libc

## add Functional

- [cacosl, cacoshl, casinl, casinhl, catanl, catanhl, ccosl, ccoshl, cexpl, clogl, csinl, csinhl, csqrtl and cpowl](../../../core/libs/prx/libc/src/FloatBuiltins.cpp) (libc) - the `long double` forms compute through `std::complex<long double>` as C99 Annex G defines them, including the branch cuts. `ctanhl` and `ctanl` are in [ComplexTanh.cpp](../../../core/libs/prx/libc/src/ComplexTanh.cpp), FreeBSD 11.4's [`s_ctanh.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/s_ctanh.c) in `long double`, switching to `±1` from a real part of magnitude 26 instead of FreeBSD's 22 for `double`: `tanhl` rounds to 1 from about 22.9, and 26 keeps FreeBSD's margin of about 3. The console's results at the branch cuts and for special values are unverified.
