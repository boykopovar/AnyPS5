# libc

## add Functional

- [cacos, cacosh, casin, casinh, catan, catanh, ccos, ccosh, cexp, clog, csin, csinh, csqrt, ctan, ctanh and cpow](../../../core/libs/prx/libc/src/FloatBuiltins.cpp) (libc) - the `double` forms compute through `std::complex<double>` as C99 Annex G defines them, including the branch cuts. The `float` and `long double` forms are not exported yet. The console's results at the branch cuts and for special values are unverified.
