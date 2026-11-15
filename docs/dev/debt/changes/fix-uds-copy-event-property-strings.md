# libSceNpUniversalDataSystem

## add Functional

- [Event property strings and binary values](../../../core/libs/prx/libSceNpUniversalDataSystem/src/EventPropertyObject.cpp) (libSceNpUniversalDataSystem) - the `sceNpUniversalDataSystemEventPropertyObjectSet*` keys and the `EventPropertyObjectSetString`/`EventPropertyArraySetString` values are copied with `CopyGuestString_nid_no_patch` with no length limit, as the direct read had none. The `SetBinary` bytes are copied with `GuestCopyIn_nid_no_patch`. An unreadable key returns `SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_KEY` and an unreadable value `SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_NULL_VALUE`, the codes a null one returns, and nothing is stored. These are host-safety fallbacks; what the console returns for unmapped strings and bytes is unmeasured.
