# libSceFont

## add Functional

- [sceFontCreateGraphicsService guest output](../../../core/libs/prx/libSceFont/src/Graphics.cpp) (libSceFont) - returning FONT_GRAPHICS_ERROR_INVALID_PARAMETER with caller errno preserved for an unwritable service pointer via GuestCopyOut_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
