# libSceNpUniversalDataSystem

## add Functional

- [sceNpUniversalDataSystemIntCreateRecordArray, sceNpUniversalDataSystemIntCreateRecordObject](../../../core/libs/prx/libSceNpUniversalDataSystem/src/Record.cpp) (libSceNpUniversalDataSystem) - returning `INVALID_ARGUMENT` for unwritable record destination pointer via `GuestCopyOut_nid_no_patch` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
