# libc

## add Functional

- [getsubopt](../../../core/libs/prx/libc/src/GetSubOpt.cpp) reads and writes guest `optionp`, `tokens`, `valuep`, and option buffer strings through `GuestCopyIn_nid_no_patch`, `GuestCopyOut_nid_no_patch`, and `GuestWritable_nid_no_patch`, preserving `errno` and failing through `NotImplemented_nid_no_patch` on unreadable or read-only guest addresses.
