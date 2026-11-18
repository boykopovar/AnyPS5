# libSceFont

## add Functional

- [sceFontCharacterGetBidiLevel](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - returning `INVALID_PARAMETER` for an unwritable bidi level output via `CopyOutGuest` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
