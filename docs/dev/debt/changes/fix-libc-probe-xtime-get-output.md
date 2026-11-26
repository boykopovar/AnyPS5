# libc

## add Unknown function info

- [xtime_get guest output](../../../core/libs/prx/libc/src/TimeExtras.cpp) (libc) - the wall-clock time is written with `GuestCopyOut_nid_no_patch`; for a non-null unwritable output the call returns 0, its result for a null output or an unsupported base, with `errno` untouched. This is a host-safety fallback; the console module writes in user space, and whether it faults on unmapped memory is unmeasured.
