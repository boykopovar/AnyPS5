# libSceNpManager

## add Functional

- [sceNpManagerIntGetNpEnv](../../../core/libs/prx/libSceNpManager/Export.cpp) (libSceNpManager) - returning SCE_NP_ERROR_INVALID_ARGUMENT for unwritable environment pointers via GuestCopyOut_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
