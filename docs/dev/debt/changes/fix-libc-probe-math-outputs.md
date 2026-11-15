# libc

## add Unknown function info

- [modf, modff, modfl, remquo, remquof, remquol and sincosf output pointers](../../../core/libs/prx/libc/src/Math.cpp) (libc) - the integral part, quotient bits or sine and cosine are written with `GuestCopyOut_nid_no_patch`; these functions have no error result, so an unwritable output is skipped and the return value is unchanged, which is a host-safety fallback; whether the console module faults on unmapped memory is unmeasured.
