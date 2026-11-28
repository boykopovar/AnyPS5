# libSceJxr

## add Functional

- [readIS, writeIS, detachISRead and detachISWrite guest pointers](../../../core/libs/prx/libSceJxr/Export.cpp) (libSceJxr) - `io` and `io->stream` are read and updated through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno` and throwing `APS5_INVALID_ARG_EX` on unreadable or unwritable pointers as a host-safety fallback instead of faulting on unmapped memory. Console behaviour on unmapped memory is unmeasured.
