# libc

## add Functional

- [__isfinite, __isfinitef, __isnan, __isnanf, __isnormal, __isnormalf, __isinf, __isinff, isnan, isnanf and isinf](../../../core/libs/prx/libc/src/Math.cpp) (libc) classify binary64 and binary32 by integer exponent and fraction fields without changing errno or floating-point exception flags, including signaling NaNs, following FreeBSD 11.4 [s_isfinite.c](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/msun/src/s_isfinite.c), [s_isnan.c](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/msun/src/s_isnan.c) [s_isnormal.c](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/msun/src/s_isnormal.c) and libc [isinf.c](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/libc/gen/isinf.c). Console exception-flag behaviour is unmeasured.
