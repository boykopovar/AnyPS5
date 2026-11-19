# libSceFont

## add Functional

- [sceFontWritingLineGetRenderMetrics and sceFontWritingLineGetOrderingSpace guest outputs](../../../core/libs/prx/libSceFont/src/WritingLine.cpp) (libSceFont) - every store, including the clears on the error paths, goes through `Font::CopyOutGuest`; returning `SCE_FONT_ERROR_INVALID_PARAMETER` for an unwritable non-null output (the error for a null metrics pointer; the ordering-space outputs may be null) is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
