# libc

## change [tanhf]

- [tanhf](../../../core/libs/prx/libc/src/Math.cpp) (libc) is FreeBSD msun's [`s_tanhf.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_tanhf.c) over a full port of msun's [`s_expm1f.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_expm1f.c) (`Expm1f`, with `huge` read at run time), which the exported `expm1f` also uses. It sets no `errno`, as the `libc.prx` that titles ship does not. The middle range still differs on some inputs under directed rounding between the bundled PPSA28050 and PPSA26344 copies. The tiny-input and finite saturation paths match both on sampled results and MXCSR with all four rounding modes and DAZ/FTZ settings. It gives the same bits as the `tanhf` of the `libc.prx` bundled with PPSA07923 and PPSA05916 for all 2^32 arguments, run on a Windows host; the FMA-contracted copy of PPSA13934 differs by up to 3 ulp on 150,846 of them.
