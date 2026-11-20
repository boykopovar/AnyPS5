# libSceFont

## add Unknown function info

- [sceFontDestroyString and sceFontDestroyWritingLine guest handles](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - the handle is read with `Font::CopyInGuest` and cleared with `Font::CopyOutGuest` before the string or line is freed, so an unwritable handle pointer frees nothing; returning `SCE_FONT_ERROR_INVALID_PARAMETER` (the null-pointer error) for an unreadable or unwritable handle pointer is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
