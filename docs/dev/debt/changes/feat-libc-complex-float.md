# libc

## add Functional

- [cacosf, cacoshf, casinf, casinhf, catanf, catanhf, ccosf, ccoshf, cexpf, clogf, csinf, csinhf, csqrtf, ctanf, ctanhf and cpowf](../../../core/libs/prx/libc/src/FloatBuiltins.cpp) (libc) - `ctanf` and `ctanhf` evaluate the reduced angle and hyperbolic terms in double precision. The remaining float forms use `std::complex<float>`. The console's results at the branch cuts and for special values are unverified.
