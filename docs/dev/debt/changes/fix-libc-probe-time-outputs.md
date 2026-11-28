# libc

## add Unknown function info

- [time and timespec_get guest outputs](../../../core/libs/prx/libc/src/Time.cpp) (libc) - `time` and `timespec_get` write their optional or required output with `GuestCopyOut_nid_no_patch`; an unwritable `time` output returns -1 with `errno` set to `EFAULT` (as `asctime_r` in the same file does for an unwritable buffer), and an unwritable `timespec_get` output returns 0, its failure result, with `errno` kept. These are host-safety fallbacks; the console module writes in user space, and whether it faults on unmapped memory is unmeasured.
