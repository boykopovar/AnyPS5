# libc

## add Functional

- [erff and erfcf](../../../core/libs/prx/libc/src/Math.cpp) (libc) lack floating-point exception comparison and a new exhaustive replay after the expf prerequisite refresh. They follow the older fdlibm [s_erff.c](https://github.com/freebsd/freebsd-src/blob/59bc2f095289a10b8cfa55cbb2b45c3bd6c2d4ae/lib/msun/src/s_erff.c), calling [expf](../../../core/libs/prx/libc/src/MathExpf.cpp). Earlier default-rounding Windows replays matched PPSA07923/PPSA05916 result bits and errno over all 2^32 inputs: erfcf set ERANGE for 10.171875 <= x < 28, and erff left errno unchanged. Those exhaustive results are historical; the refreshed branch has scoped Linux SDK/CPU validation. The FMA-contracted PPSA13934 build differed on 113,414,840 erff and 2,948,992 erfcf arguments. Labels: tested, console, windows
