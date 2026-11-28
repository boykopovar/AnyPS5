# libc

## add Functional

- [wcstok_s guest pointers](../../../core/libs/prx/libc/src/BoundsChecking.cpp) (libc) - `remaining`, `context`, `delimiters` and `str` are read and written through `GuestCopyIn_nid_no_patch`, `GuestCopyOut_nid_no_patch`, `GuestWritable_nid_no_patch` and `GuestArena::HostWrite`, preserving host `errno`; an unreadable or unwritable pointer is reported as the same "bad argument" runtime-constraint violation (EINVAL) a null pointer gets without modifying guest memory, which is a host-safety fallback; whether the console module faults on unmapped or read-only memory is unmeasured.
