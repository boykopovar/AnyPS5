# libSceFont

## add Functional

- [sceFontStringRefersTextCharacters guest output](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - returning nullptr with caller errno preserved for an unwritable characterCount pointer via GuestCopyOut_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
