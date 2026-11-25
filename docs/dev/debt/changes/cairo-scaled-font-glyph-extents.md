# libcairo

## add Functional

- [ScaledFont.cpp](../../../core/libs/prx/libcairo/ScaledFont.cpp) - `cairo_scaled_font_glyph_extents` queries bounding box and advance metrics for an array of glyphs through the native Cairo backend. Glyph pointers and extents output buffers are checked against guest memory boundaries before forwarding to native Cairo. Unverified on console.

## remove | `cairo_scaled_font_glyph_extents` | `oy7NcAkex1I` |
