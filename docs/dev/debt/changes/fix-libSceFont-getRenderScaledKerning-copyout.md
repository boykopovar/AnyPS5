# libSceFont

## add Functional

- [sceFontGetRenderScaledKerning](../../../core/libs/prx/libSceFont/src/Render.cpp) (libSceFont) - returning `SCE_FONT_ERROR_INVALID_PARAMETER` for an unwritable `kerning` pointer via `CopyOutGuest` with caller `errno` preserved is a host-safety fallback with console behavior unmeasured. Closes when: measured on hardware. Labels: console, tested.
