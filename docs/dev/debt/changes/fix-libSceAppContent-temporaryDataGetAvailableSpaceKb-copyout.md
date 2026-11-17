# libSceAppContent

## add Functional

- [sceAppContentTemporaryDataGetAvailableSpaceKb](../../../core/libs/prx/libSceAppContent/Export.cpp) (libSceAppContent) - returning `PARAMETER` for unwritable guest destination pointer via `GuestCopyOut_nid_no_patch` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
