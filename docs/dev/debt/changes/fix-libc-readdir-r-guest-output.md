# libc

## add Functional

- [readdir_r guest output](../../../core/libs/prx/libc/src/Directory.cpp) (libc) - returning 14 (EFAULT) with the caller's errno preserved for an unwritable entry or result pointer via GuestCopyOut_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
