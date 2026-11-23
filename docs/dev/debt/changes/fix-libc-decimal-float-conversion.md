# libc

## add Functional

- [Dinkumware single-precision decimal conversion](../../../core/libs/prx/libc/src/DinkumwareInternals.cpp) uses nine significant digits, a sticky discarded tail and three float parts for the decimal scale. The `libc.prx` copies bundled with PPSA28050 and PPSA26344 agree on the result, end pointer, error output, guest errno and MXCSR flags across the four rounding modes on a Linux host. Both title libraries stop accumulating exponent magnitude after it exceeds 99,999,999, consume the remaining digits and combine the exponent and caller scale modulo 2^64. At the largest scales these choices can reverse the direction of a range error. The narrow and wide `_Stof` wrappers use the same conversion. Other SDK versions, console execution and non-C numeric locales are unverified.
