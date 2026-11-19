# libSceFont

## add Functional

- [sceFontAttachDeviceCacheBuffer guest buffer](../../../core/libs/prx/libSceFont/src/Library.cpp) (libSceFont) - returning SCE_FONT_ERROR_INVALID_PARAMETER with caller errno preserved and no cache attached when the caller's buffer cannot take the 24-byte cache header is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured. Only the header is probed, not the rest of the `size` bytes.
