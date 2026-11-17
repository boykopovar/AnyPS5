# libSceAppContent

## add Functional

- [sceAppContentGetAddcontInfoListByIroTag](../../../core/libs/prx/libSceAppContent/Export.cpp) (libSceAppContent) - returning `SCE_APP_CONTENT_ERROR_PARAMETER` for an unwritable hit-count output pointer via `GuestCopyOut_nid_no_patch` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
