# libc

## add Functional

- [Dinkumware single-precision decimal conversion](../../../core/libs/prx/libc/src/DinkumwareInternals.cpp) uses nine significant digits, a sticky discarded tail and three float parts for the decimal scale. The `libc.prx` copies bundled with PPSA28050 and PPSA26344 agree on the result, end pointer, error output, guest errno and MXCSR flags across the four rounding modes on a Linux host. The narrow and wide `_Stof` wrappers use the same conversion. Other SDK versions, console execution and non-C numeric locales are unverified.
