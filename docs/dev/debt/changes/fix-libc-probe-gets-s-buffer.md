# libc

## add Unknown function info

- [gets_s guest buffer](../../../core/libs/prx/libc/src/BoundsChecking.cpp) (libc) - each character and the terminator are written with `GuestCopyOut_nid_no_patch`; an unwritable buffer is reported as the same "bad argument" runtime-constraint violation (EINVAL) a null buffer gets, and the rest of the line is read and discarded as on the other violations, which is a host-safety fallback; whether the console module faults on unmapped memory is unmeasured.
