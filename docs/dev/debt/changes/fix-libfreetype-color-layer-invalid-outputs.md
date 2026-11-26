# libfreetype

## add Functional

- [FT_Get_Color_Glyph_Layer invalid layers](../../../core/libs/prx/libfreetype/src/Color.cpp) (libfreetype) - when a COLR layer record names a glyph at or past `num_glyphs`, or a palette index other than `0xFFFF` at or past `num_palette_entries`, the export returns 0 and still writes that record's glyph and colour indices, as [FreeType 2.15.0 ttcolr.c](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/sfnt/ttcolr.c#L486-L492) writes them before it validates them. The console module's behaviour is unmeasured.
