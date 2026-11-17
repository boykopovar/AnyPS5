# libSceFont

## add Functional

- [sceFontCharacterGetTextOrder](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - returning `INVALID_PARAMETER` for unwritable guest destination pointer via `CopyOutGuest` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
