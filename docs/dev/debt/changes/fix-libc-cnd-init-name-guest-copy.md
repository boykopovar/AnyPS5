# libc

## add Functional

- [_Cnd_init_with_name](../../../core/libs/prx/libc/src/Threads.cpp) (libc) reads at most the 32 bytes of the name its limit allows, through `CopyGuestString_nid_no_patch`, so an unreadable name fails with `errno` `EFAULT` (14) before a condition is made. A name of 32 or more characters still fails with `ENAMETOOLONG` (63). This is a host-safety fallback: the console's result for an inaccessible name is not measured. Closes when: compared with the console.
