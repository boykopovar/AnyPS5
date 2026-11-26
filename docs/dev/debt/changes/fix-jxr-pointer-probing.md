# libSceJxr

## add Functional

- [PKFree_nid_postfix, CreateWS_Memory_nid_postfix, PKCodecFactory_CreateFormatConverter_nid_postfix, PKCreateFactory_nid_postfix](../../../core/libs/prx/libSceJxr/Export.cpp), and [attachISWrite_nid_postfix](../../../core/libs/prx/libSceJxr/src/BitIo.cpp) (libSceJxr) - probing guest-supplied pointers via `GuestWritable_nid_no_patch`, `GuestCopyIn_nid_no_patch`, and `GuestCopyOut_nid_no_patch` while preserving `errno` and throwing `std::invalid_argument` on unreadable or unwritable pointers is a host-safety fallback; console behaviour on invalid JXR pointers is unmeasured.
