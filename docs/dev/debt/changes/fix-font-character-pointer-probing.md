# libSceFont

## add Functional

- [sceFontStringRefersRenderCharacters, sceFontCharacterGetTextFontCode, and sceFontCharacterGetTextOrder](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - probing `FontTextCharacter` inputs via `Font::CopyInGuest` with bounds, alignment, and step-count checks while preserving `errno` and zeroing outputs on failure is a host-safety fallback; console behaviour on invalid or corrupted character pointers is unmeasured.
