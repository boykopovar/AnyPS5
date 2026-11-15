# libSceNpUniversalDataSystem

## add Functional

- [sceNpUniversalDataSystemIntCreateRecordData](../../../core/libs/prx/libSceNpUniversalDataSystem/src/Record.cpp) (libSceNpUniversalDataSystem) - returning kInvalidArgument for unwritable record data pointers via GuestCopyOut_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
