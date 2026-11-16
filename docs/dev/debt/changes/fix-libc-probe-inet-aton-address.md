# libc

## add Unknown function info

- [__inet_aton destination](../../../core/libs/prx/libc/src/StringExtensions.cpp) (libc) - the parsed address is written with `GuestCopyOut_nid_no_patch`; for a non-null unwritable destination the call returns 0, its invalid-address result, with `errno` untouched. This is a host-safety fallback; the console module writes in user space, and whether it faults on unmapped memory is unmeasured.

# libSceLibcInternal

## add Unknown function info

- [inet_aton destination](../../../core/libs/prx/libSceLibcInternal/Inet.cpp) (libSceLibcInternal) - the parsed address is written with `GuestCopyOut_nid_no_patch`; for a non-null unwritable destination the call returns 0, its invalid-address result, with `errno` untouched. This is a host-safety fallback; the console module writes in user space, and whether it faults on unmapped memory is unmeasured.
