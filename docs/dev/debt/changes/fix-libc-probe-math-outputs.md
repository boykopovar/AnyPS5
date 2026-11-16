# libc

## add Unknown function info

- [modfl and remquol output pointers](../../../core/libs/prx/libc/src/MathExact.cpp) (libc) - the integral part and the quotient bits are written with `GuestCopyOut_nid_no_patch`; these functions have no error result, so an unwritable output is skipped and the return value is unchanged, which is a host-safety fallback; whether the console module faults on unmapped memory is unmeasured. The float and double versions (`modf`, `modff`, `remquo`, `remquof`, `sincosf`) keep the direct store because they run per element in game math loops and the probe costs a syscall per call.
