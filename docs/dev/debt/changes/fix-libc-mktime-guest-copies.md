# libc

## add Functional

- [mktime, timelocal](../../../core/libs/prx/libc/src/Time.cpp) (libc) copy the caller's `struct tm` in with `GuestCopyIn_nid_no_patch` and write the normalized result back with `GuestCopyOut_nid_no_patch`; `timelocal` writes its `tm_isdst = -1` the same way. A null `struct tm`, or one that can't be read or written, returns -1 with `errno` set to `EFAULT`, as `time` does for an output it can't write. This is a host-safety fallback: FreeBSD dereferences the pointer, and the console's behaviour for an inaccessible one is not measured.
