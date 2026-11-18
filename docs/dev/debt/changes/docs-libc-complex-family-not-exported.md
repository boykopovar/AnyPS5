# libc

## change [creal, crealf, creall, cimag, cimagf, cimagl, conj, conjf, conjl, cproj, cprojf, cprojl, carg, cargf, cargl, cabs, cabsf and cabsl]

- [creal, crealf, creall, cimag, cimagf, cimagl, conj, conjf, conjl, cproj, cprojf, cprojl, carg, cargf, cargl, cabs, cabsf and cabsl](../../../core/libs/prx/libc/src/FloatBuiltins.cpp) (libc) - follow FreeBSD 11 msun and C99: `creal`/`cimag` extract components, `conj` conjugates the imaginary sign, `cproj` projects infinities to the Riemann sphere with signed zero imaginary part, `carg` delegates to `atan2*` and `cabs` delegates to `hypot*` (see `atan2l`/`hypotl` for host-libm long double precision); firmware 11.60's `libSceLibcInternal.sprx` exports none of the 18 (`dlsym` in a PS5 payload process), and a title's own `libc.prx` is unmeasured
