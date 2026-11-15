# libSceNpUniversalDataSystem

## add Functional

- [sceNpUniversalDataSystemEventEstimateSize](../../../core/libs/prx/libSceNpUniversalDataSystem/src/Event.cpp) (libSceNpUniversalDataSystem) - returning SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT for unwritable size pointers via GuestCopyOut_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
