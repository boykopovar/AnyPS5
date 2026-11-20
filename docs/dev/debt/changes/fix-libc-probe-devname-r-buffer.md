# libc

## add Unknown function info

- [devname_r buffer](../../../core/libs/prx/libc/src/DeviceName.cpp) (libc) - the name (or `#NODEV`) and its terminator, and the empty string of the overflow path, are written with `GuestCopyOut_nid_no_patch`; an unwritable buffer is skipped and the buffer pointer is still returned (the overflow path still sets `EOVERFLOW`), because `devname_r` has no error channel and its behaviour on unmapped memory is unmeasured.
