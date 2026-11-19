# libSceJxr

## add Functional

- [PKAllocAligned and PKImageDecode_Release guest output pointers](../../../core/libs/prx/libSceJxr/src/GlueHelpers.cpp) (libSceJxr) - `memory` and `decoder` pointer slots are read and written through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno` and throwing `std::invalid_argument` on unreadable or unwritable pointer slots as a host-safety fallback instead of faulting on unmapped memory. Console behaviour on unmapped memory is unmeasured.
