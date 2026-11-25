# libSceJxr

## add Functional

- [PKFree, CreateWS_Memory, PKCodecFactory_CreateFormatConverter, PKCreateFactory and attachISWrite guest pointers](../../../core/libs/prx/libSceJxr/Export.cpp) (libSceJxr) - `PKFree`, `CreateWS_Memory`, `PKCodecFactory_CreateFormatConverter`, `PKCreateFactory` and `attachISWrite` probe and copy guest pointers through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno`, throwing `std::invalid_argument` on unreadable or unwritable guest memory as a host-safety fallback instead of faulting on unmapped pointers. Console behaviour on unmapped memory is unmeasured.
