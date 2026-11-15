# libSceNet

## add Functional

- [sceNetSyncWait](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - returning early on unwritable timeout pointers via GuestCopyIn_nid_no_patch and GuestWritable_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
