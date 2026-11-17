# libSceNpUniversalDataSystem

## add Functional

- [sceNpUniversalDataSystemEventEstimateSize](../../../core/libs/prx/libSceNpUniversalDataSystem/src/Event.cpp) (libSceNpUniversalDataSystem) - returning `INVALID_ARGUMENT` for an unwritable size output via `GuestCopyOut_nid_no_patch` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
