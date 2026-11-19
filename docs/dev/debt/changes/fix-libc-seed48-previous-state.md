# libc

## add Functional

- [seed48](../../../core/libs/prx/libc/src/Random.cpp) (libc) returns the previous 48-bit state from its internal result buffer before installing the caller's seed and restores the standard multiplier and addend, following the [POSIX `drand48` family contract](https://pubs.opengroup.org/onlinepubs/9699919799/functions/drand48.html). This return-value behavior has not been measured against a PS5 title `libc.prx` or the system `libSceLibcInternal`.
