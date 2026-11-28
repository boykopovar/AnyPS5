# libc

## add Unknown function info

- [a64l and l64a_r guest buffers](../../../core/libs/prx/libc/src/Radix64.cpp) (libc) - `a64l` reads its at most six digits with `GuestCopyIn_nid_no_patch` and returns 0, its null-string result, for an unreadable string; `l64a_r` writes each digit and the terminator with `GuestCopyOut_nid_no_patch` and returns -1, its failure result, for an unwritable buffer, keeping the digits already written. `errno` is untouched. These are host-safety fallbacks; the console module accesses the buffers in user space, and whether it faults on unmapped memory is unmeasured.
