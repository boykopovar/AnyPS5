# libSceFont

## add Functional

- [sceFontWordsFindWordCharacters guest output](../../../core/libs/prx/libSceFont/src/Words.cpp) (libSceFont) - returning SCE_FONT_ERROR_INVALID_PARAMETER with caller errno preserved for unwritable lastCharacter or nextCharacter pointers via GuestCopyOut_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
