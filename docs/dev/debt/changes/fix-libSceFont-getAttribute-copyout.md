# libSceFont

## add Functional

- [sceFontGetAttribute guest output](../../../core/libs/prx/libSceFont/src/Style.cpp) (libSceFont) - returning SCE_FONT_ERROR_INVALID_PARAMETER for an unwritable attribute pointer is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
