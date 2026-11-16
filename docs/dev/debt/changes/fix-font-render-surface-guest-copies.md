# libSceFont

## add Functional

- [Render surface setup](../../../core/libs/prx/libSceFont/src/Render.cpp) (libSceFont) - `sceFontRenderSurfaceInit`, `sceFontRenderSurfaceSetScissor` and `sceFontRenderSurfaceSetStyleFrame` read and write the caller's `FontRenderSurface` (and the style frame's magic) through checked copies, writing only the fields they set. An unmapped surface leaves the two void calls without effect and makes `SetStyleFrame` return `SCE_FONT_ERROR_INVALID_PARAMETER`; the console's behaviour for it is not measured.
