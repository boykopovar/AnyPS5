# libc

## change [atanf]

- [atanf](../../../core/libs/prx/libc/src/Math.cpp) (libc) is FreeBSD msun's [`s_atanf.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_atanf.c), with `atanlo[3]` read at run time as msun's `volatile` does. Tiny arguments raise `FE_INEXACT` as FreeBSD msun does. It gives the same bits as the `atanf` of the `libc.prx` bundled with PPSA07923, PPSA05916 and PPSA13934 for all 2^32 arguments, run on a Windows host, and none of those sets `errno`. Whether other titles' builds differ is unverified.

## change [asinf]

- [asinf](../../../core/libs/prx/libc/src/Math.cpp) (libc) - for |x| > 1, the infinities and NaN, it stores `EDOM` before returning `(x - x) / (x - x)`, as the `libc.prx` bundled with eleven titles does, PPSA13934's included. Tiny arguments raise `FE_INEXACT` as FreeBSD msun does. Otherwise this is FreeBSD msun's [`e_asinf.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_asinf.c), which gives the same bits as the `asinf` of the `libc.prx` bundled with PPSA07923 and PPSA05916 for all 2^32 arguments (run on a Windows host). The FMA-contracted builds bundled with PPSA13934 and PPSA09614 differ from it by 1 ulp on 391,714 arguments, so no single port matches every title's last bit.
