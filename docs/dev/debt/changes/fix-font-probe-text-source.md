# libSceFont

## add Unknown function info

- [sceFontTextSourceInit, sceFontTextSourceRewind, sceFontTextSourceSetDefaultFont and sceFontTextSourceSetWritingForm guest pointers](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - the text source is copied in, checked and updated on the host and copied out with errno preserved (`sceFontTextSourceInit` still clears it before checking the parser); returning `SCE_FONT_ERROR_INVALID_PARAMETER` (the null-pointer error) for an unreadable or unwritable text source is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
