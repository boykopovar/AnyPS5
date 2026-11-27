# libSceFreeType

## add Unknown function info

- [FT_Glyph_To_Bitmap, FT_Glyph_Transform, FT_Glyph_Stroke and FT_Glyph_StrokeBorder](../../../core/libs/prx/libSceFreeType/src/Glyph.cpp) (libSceFreeType) - console prototypes, FreeType version and error equivalence are unverified. The adapters use bundled [FreeType 2.15.0](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftglyph.c) and [ftstroke.c](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftstroke.c) through the registered glyph owner. Transform matrices and coordinates are range-checked against host field widths; outline points are copied into and republished from the host glyph. Replacing operations retain the original guest handle when `destroy` is false and publish a separately owned result. The console version's rounding, rasterization and ownership behavior has not been measured.

# libSceFreeTypeOt

## remove - The FT_Glyph_Transform, FT_Glyph_StrokeBorder, FT_Glyph_To

## add Pending dependency exports

- The FT_New_Library NID (`yrtsLmTfa48`) in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) is identified, but its provider-specific prototype and behavior remain unverified. Its named helper retains the explicit inventory alias and throws.

- The FT_Glyph_Stroke, FT_Glyph_StrokeBorder, FT_Glyph_To_Bitmap and FT_Glyph_Transform NIDs (`0rKmI1ZbMAc`, `BBRhYfYBDgU`, `c1nbr8oSWlg`, `AVEEb57HhUw`) in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) forward to the shared guest glyph owner in [libSceFreeType](../../../core/libs/prx/libSceFreeType/src/Glyph.cpp). Their signatures follow the bundled FreeType 2.15.0 headers; the console's exact prototypes, version, errors, rasterization and ownership behavior remain unverified.
