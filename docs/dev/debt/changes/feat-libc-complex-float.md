# libc

## add Functional

- [cacosf, cacoshf, casinf, casinhf, catanf, catanhf, ccosf, ccoshf, cexpf, clogf, csinf, csinhf, csqrtf, ctanf, ctanhf and cpowf](../../../core/libs/prx/libc/src/FloatBuiltins.cpp) (libc) - `catanf`, `catanhf`, `ctanf`, `ctanhf` and `clogf` evaluate their intermediate terms in double precision: `log1p` and `atan2` near the unit circle, and a reduced angle for the tangents. The other float forms use `std::complex<float>` with the C99 Annex G branch cuts. The console's results at the branch cuts and for special values are unverified.
