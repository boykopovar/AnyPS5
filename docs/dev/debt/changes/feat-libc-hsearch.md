# libc

## add Functional

- [hcreate, hsearch and hdestroy](../../../core/libs/prx/libc/src/HashSearch.cpp) (libc) - console behavior is unverified. Implemented from POSIX.1-2008 as one open-addressed table with FNV-1a hashing, whose slots hold the caller's key and data pointers. The table has the next power of two at or above twice `nel`, and at least 16 slots, so `hcreate(0)` succeeds and a full table returns `ENOMEM` only when every slot is used, as the pinned OpenBSD reference rounds `nel` up. Keys are read through the guest string copy, up to 4096 bytes. A second `hcreate` without `hdestroy` returns 0 with `EEXIST`, and `hsearch` without a table returns null with `ESRCH`; POSIX leaves both unspecified.
