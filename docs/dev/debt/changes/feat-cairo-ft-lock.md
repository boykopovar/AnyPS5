# libcairo

## add Functional glyphs

- [FreeType scaled fonts](../../../core/libs/prx/libcairo/ScaledFont.cpp) - `cairo_ft_scaled_font_lock_face` returns null and `cairo_ft_scaled_font_unlock_face` does nothing, as Cairo 1.18.0 does for a scaled font outside its FreeType backend. The bundled Cairo has no FreeType backend, so this holds for every scaled font here; a font that reports `CAIRO_FONT_TYPE_FT` throws. On the console, toy faces resolve through FreeType, so locking their scaled fonts would return a face. A null font throws because Cairo dereferences it.

## remove `4gDTaBZH8lo`

## remove `SJMLSxaYvWU`
