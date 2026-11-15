# libc

## add Unknown function info

- [basename_r buffer](../../../core/libs/prx/libc/src/StringExtensions.cpp) (libc) - the last path component is written with `GuestCopyOut_nid_no_patch`; for a non-null unwritable buffer the call returns null with `errno` 14 (`EFAULT`), the same error channel as its `ENAMETOOLONG` result. This is a host-safety fallback; the console module writes in user space, and whether it faults on unmapped memory is unmeasured. The path is still read in place.
