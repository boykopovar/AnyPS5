# libSceFont

## add Unknown function info

- [sceFontCharacterGetTextFontCode, sceFontCharacterGetTextOrder and sceFontStringRefersRenderCharacters guest inputs](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - `sceFontCharacterGetTextFontCode` and `sceFontCharacterGetTextOrder` copy `textCharacter` via `Font::CopyInGuest`, clearing valid outputs and returning `SCE_FONT_ERROR_INVALID_PARAMETER` on unmapped input with caller `errno` preserved; `sceFontStringRefersRenderCharacters` validates `startCharacter` and `lastCharacter` alignment and bounds against the string's character array before indexing, returning `nullptr` and zeroing `*characterCount` on out-of-range or unaligned pointers. Console behaviour on unmapped or unaligned guest character pointers is unmeasured.
