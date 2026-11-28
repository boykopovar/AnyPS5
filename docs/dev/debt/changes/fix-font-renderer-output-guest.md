# libSceFont

## add Unknown function info

- [Renderer creation guest output](../../../core/libs/prx/libSceFont/src/Library.cpp): `sceFontCreateRendererWithEdition` and `sceFontCreateRenderer` check and clear a non-null guest output before allocating the renderer, reject an inaccessible output with `SCE_FONT_ERROR_INVALID_PARAMETER` without consuming allocator callbacks, and clean up if the final output write fails. The error returned for an inaccessible output is a host-safety fallback, not a measured console rule.
