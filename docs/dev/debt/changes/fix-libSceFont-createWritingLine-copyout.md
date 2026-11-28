# libSceFont

## add Functional

- [sceFontCreateWritingLine guest output](../../../core/libs/prx/libSceFont/src/WritingLine.cpp) (libSceFont) - returning SCE_FONT_ERROR_INVALID_PARAMETER with caller errno preserved for an unwritable pWritingLine pointer via GuestCopyOut_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
