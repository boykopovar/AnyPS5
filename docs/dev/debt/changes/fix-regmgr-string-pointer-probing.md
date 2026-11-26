# libSceRegMgr

## add Functional

- [sceRegMgrStrncpy and sceRegMgrMemset](../../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - probing guest memory via `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` and returning `1` with `errno` preserved on unreadable or unwritable pointers is a host-safety fallback; console behaviour on unmapped pointers is unmeasured.
