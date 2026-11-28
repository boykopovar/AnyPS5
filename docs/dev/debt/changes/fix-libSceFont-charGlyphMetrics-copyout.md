# libSceFont

## add Functional

- [sceFontGetCharGlyphMetrics, sceFontGetRenderCharGlyphMetrics and ClearRenderOutputs guest outputs](../../../core/libs/prx/libSceFont/src/Layout.cpp) (libSceFont) - glyph metrics and render output destinations are validated and written using CopyOutGuest instead of direct host pointer dereferences; returning SCE_FONT_ERROR_INVALID_PARAMETER on unwritable destination is host safety fallback with console behaviour unmeasured.
