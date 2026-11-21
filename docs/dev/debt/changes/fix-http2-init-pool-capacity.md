# libSceHttp2

## change [sceHttp2Init]

- [sceHttp2Init](../../../core/libs/prx/libSceHttp2/Export.cpp) - the export omits the low-stack return (0x817B1076) and the call to sceNetCtlInit. SSL capacity uses host mbedTLS allocation totals, which can differ from console pool use. HTTP pool statistics report the rounded mapping size and zero usage; firmware queries the mspace allocator. Scheduling with a negative maximum is unmeasured. The pool limits and context creation follow firmware 13.40 libSceHttp2.sprx (wrapper 0xe0, helper 0x5960). On firmware 12.70 a 64 KiB pool returned 0x817B11FE for count 0 and 0x817B121D for count 1. Neither count -1 with 64 KiB nor count 1 with 1 MiB returned before the runner timed out.
