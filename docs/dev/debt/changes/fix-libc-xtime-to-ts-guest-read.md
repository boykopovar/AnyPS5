# libc

## add Functional

- [_Xtime_to_ts](../../../core/libs/prx/libc/src/TimeExtras.cpp) (libc) - the guest `xtime` is copied with `GuestCopyIn_nid_no_patch`, as `_Xtime_diff_to_ts` does; a null or unreadable pointer throws, because what the console module does on unmapped memory is unmeasured.
