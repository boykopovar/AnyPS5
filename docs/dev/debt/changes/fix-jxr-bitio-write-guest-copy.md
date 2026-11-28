# libSceJxr

## add Functional

- [attachISWrite, putBit16z and fillToByte guest pointers](../../../core/libs/prx/libSceJxr/src/BitIo.cpp) (libSceJxr) - `io` and `stream` are read and validated for writability through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno`, throwing `std::invalid_argument` on unreadable or unwritable pointers as a host-safety fallback instead of faulting on unmapped memory. Console behaviour on unmapped memory is unmeasured.
