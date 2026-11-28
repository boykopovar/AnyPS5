# libc

## add Unknown function info

- [_Strcoll, _Strxfrm](../../../core/libs/prx/libc/src/CLocaleHelpers.cpp) (libc) - inverted ranges, null non-empty ranges, unreadable source ranges, and unwritable destination ranges throw `std::invalid_argument` after `GuestArenaPrepareHostRead_nid_no_patch` / `GuestReadable_nid_no_patch` / `GuestArena::HostWrite` / `GuestWritable_nid_no_patch` validation while preserving `errno`; console fault behavior for invalid guest pointers is not measured.
