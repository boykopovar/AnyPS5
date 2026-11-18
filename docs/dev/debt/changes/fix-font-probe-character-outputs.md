# libSceFont

## add Unknown function info

- [sceFontCharacterGetBidiLevel, sceFontCharacterGetTextFontCode and sceFontCharacterGetTextOrder guest outputs](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - the outputs, including the values cleared for a null character, are written with `Font::CopyOutGuest`; returning `SCE_FONT_ERROR_INVALID_PARAMETER` (the null-pointer error) for an unwritable output is a host-safety fallback, and console behaviour on unmapped memory is unmeasured. The character itself is still read in place.
