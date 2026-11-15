# libSceNpManager

## add Unknown function info

- [sceNpGetSandboxInfo](../../../core/libs/prx/libSceNpManager/Export.cpp) (libSceNpManager) - copies out sandbox info via GuestCopyOut_nid_no_patch with caller errno preserved; returning SCE_NP_ERROR_INVALID_ARGUMENT for an unmapped or unwritable pointer is a host-safety fallback with console behavior unmeasured.
