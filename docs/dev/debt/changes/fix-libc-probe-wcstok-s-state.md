# libc

## add Unknown function info

- [wcstok_s guest state](../../../core/libs/prx/libc/src/BoundsChecking.cpp) (libc) - `s1max` and the context are copied in, and the remaining count, the context and the terminator written after a token are copied out with checked guest copies; an unreadable or unwritable `s1max`, context or terminator slot is reported as the same "bad argument" runtime-constraint violation (EINVAL) a null pointer gets, which is a host-safety fallback; whether the console module faults on unmapped memory is unmeasured. The string and delimiters are still read in place.
