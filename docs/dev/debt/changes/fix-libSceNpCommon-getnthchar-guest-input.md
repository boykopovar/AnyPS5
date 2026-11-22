# libSceNpCommon

## add Functional

- [sceNpUtilGetNthChar guest memory fallback](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - reads `text` in 4096-byte page-bounded slices via `GuestCopyIn_nid_no_patch` (keeping `errno`) and checks `character` writability via `GuestWritable_nid_no_patch` before `GuestCopyOut_nid_no_patch`. Returning `0x80550601` (`NP_UTIL_ERROR_INVALID_ARGUMENT`) on an unreadable source or unwritable destination is a host-safety fallback with console behaviour unmeasured. Closes when: compared with the console.
