# libc

## add Functional

- [erff and erfcf](../../../core/libs/prx/libc/src/Math.cpp) (libc) do not compare floating-point exceptions. They are the fdlibm [`s_erff.c`](https://github.com/freebsd/freebsd-src/blob/59bc2f095289a10b8cfa55cbb2b45c3bd6c2d4ae/lib/msun/src/s_erff.c) that predates the FreeBSD 11.4 rewrite, calling this file's msun `expf`, so `erfcf` sets `ERANGE` exactly where one of its `expf` calls underflows (every argument from 10.171875 up to, but not including, 28) and `erff` never sets `errno`. They give the same bits and `errno` as the `erff` and `erfcf` bundled with PPSA07923 and PPSA05916 for all 2^32 arguments on a Windows host. The FMA-contracted build bundled with PPSA13934 differs on 113,414,840 `erff` and 2,948,992 `erfcf` arguments.
