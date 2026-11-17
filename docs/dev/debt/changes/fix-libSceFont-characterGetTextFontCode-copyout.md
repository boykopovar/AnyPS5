# libSceFont

## add Functional

- [sceFontCharacterGetTextFontCode](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - returning `INVALID_PARAMETER` for unwritable guest destination pointers via `CopyOutGuest` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
