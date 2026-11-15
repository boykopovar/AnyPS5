# libSceVdecCore

## add Functional

- [sceVdecCoreGetDecodeOutput](../../../core/libs/prx/libSceVdecCore/Export.cpp) (libSceVdecCore) - returning VdecCoreErrorInvalidArgument for unwritable output info pointers via GuestCopyOut_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
