# libSceRegMgr

## add Functional

- [String and memset helper guest pointers](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - `sceRegMgrStrncpy` reads source strings through `GuestCopyIn_nid_no_patch` in 4096-byte chunks with a byte-by-byte fallback on page boundaries and writes through `GuestCopyOut_nid_no_patch`, and `sceRegMgrMemset` writes through `GuestCopyOut_nid_no_patch` in 4096-byte chunks. Both preserve `errno` and return `(char*)1` or `(void*)1` on unreadable or unwritable non-null guest pointers as a host-safety fallback where the console faults.
