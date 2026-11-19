# libSceFont

## add Functional

- [sceFontDettachDeviceCacheBuffer guest output](../../../core/libs/prx/libSceFont/src/Library.cpp) (libSceFont) - returning SCE_FONT_ERROR_INVALID_PARAMETER with caller errno preserved and cache state left attached and intact for an unwritable buffer or size pointer is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
