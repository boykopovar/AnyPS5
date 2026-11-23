# libc

## add Functional

- [complex trigonometric, hyperbolic, exponential, logarithmic and power functions](../../../core/libs/prx/libc/src/UnsupportedComplex.cpp) (libc) - the `double`, `float` and `long double` families of `cacos` through `ctanh`, `cexp`, `clog` and `cpow` throw `NotImplemented`. Their complex argument and result ABI is unverified, so the exports take no typed parameters. A title that imports one now loads, and a call reports the missing function.
