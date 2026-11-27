# libc

## change [div, ldiv, lldiv and imaxdiv]

- [div, ldiv, lldiv and imaxdiv](../../../core/libs/prx/libc/src/Strings.cpp) (libc, and [Math.cpp](../../../core/libs/prx/libc/src/Math.cpp) for the 64-bit ones) - with a divisor of -1 the quotient is the two's-complement negation of the numerator and the remainder 0, so INT_MIN / -1 returns INT_MIN, where the host would fault. A PS5 (firmware 11.60, payload process, the system `libSceLibcInternal.sprx`; a title process may differ) took SIGFPE for INT_MIN / -1 in `div` and INT64_MIN / -1 in `ldiv`, `lldiv` and `imaxdiv` in one run, as the hardware division does, so the console faults where this returns the minimum. In the other seven cases run for each of the four (7 / 2, -7 / 2, 0 / -1, the maximum / -1, the minimum / 2, -5 / -1 and 5 / -1) the results were the quotients and remainders given here. A zero divisor is unchanged and keeps the zero-divisor behaviour recorded in the Integer compiler-rt builtins entry.
