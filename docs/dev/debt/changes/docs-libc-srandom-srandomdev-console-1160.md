# libc

## change [random, srandom, initstate, setstate]

- [random, srandom, initstate, setstate](../../../core/libs/prx/libc/src/Random.cpp) (libc) - a null state throws where the system `libSceLibcInternal` of PS5 firmware 13.60 faults, `initstate` below 8 bytes skips that library's "not enough state" warning, and concurrent callers (one lock here) and a title's own libc are not measured. Otherwise it follows payload processes on PS5 firmware 12.70 and 13.60 (11.60 gave the same first four `random()` values after `srandom(1)`): a fresh process draws the `srandom(1)` sequence; `srandom` seeds through `good_rand` (int32 argument, 0 becomes 123459876, `16807 * (x % 127773) - 2836 * (x / 127773)` plus 0x7fffffff when negative); `initstate` picks types 0 to 4 by size, returns null below 8 bytes without touching `errno` but still saves the current rear position; both calls return the previous state; `setstate` takes a rear position past the degree unchecked

## change [srandomdev]

- [srandomdev](../../../core/libs/prx/libc/src/Random.cpp) (libSceLibcInternal, implemented in libc) is verified against the console only in that two calls in a payload process on PS5 firmware 11.60 gave different `random()` sequences. The firmware 3.20 [export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c) names NID `a2MOZf++Wjg`; it follows FreeBSD 11 [random.c](https://github.com/freebsd/freebsd-src/blob/4ab2e064d7950be84256d671a7ae93f87cc6aa36/lib/libc/stdlib/random.c), filling every active generator word from host OS entropy instead of `KERN_ARND`, then resetting additive feedback positions without warming up. An entropy failure throws rather than leaving `sysctl`'s failure unchecked. Calls share the generator lock.
