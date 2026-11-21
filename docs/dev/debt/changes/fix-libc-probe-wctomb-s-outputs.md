# libc

## add Unknown function info

- [wctomb_s and wcrtomb_s guest outputs](../../../core/libs/prx/libc/src/BoundsChecking.cpp) (libc) - the converted bytes are built in a host buffer and copied out with `GuestCopyOut_nid_no_patch`, then the status or result; an unwritable buffer, status or result is reported as the same "bad argument" runtime-constraint violation (EINVAL) a null pointer gets (`wcrtomb_s` also stores the failed result when it can), which is a host-safety fallback; whether the console module faults on unmapped memory is unmeasured. The `wcrtomb_s` state is still read in place.
