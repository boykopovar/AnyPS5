# libSceFont

## add Functional

- [sceFontOpenFontMemory, sceFontOpenFontFile](../../../core/libs/prx/libSceFont/src/Open.cpp) (libSceFont) - returning `INVALID_PARAMETER` for unwritable handle pointers via `CopyOutGuest` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
