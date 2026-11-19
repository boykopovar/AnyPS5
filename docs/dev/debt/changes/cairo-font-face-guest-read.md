# libcairo

## change [FreeType font faces]

- [FreeType font faces](../../../core/libs/prx/libcairo/FontFace.cpp) follow Cairo 1.18.0 for lifetime only: `cairo_ft_font_face_create_for_ft_face` returns the cached face for the same guest `FT_Face`, `face_index` and load flags with one more reference, the face does not reference the `FT_Face`, and user data is released in reverse order after the face leaves the cache. Cairo's "zombie" face, kept while a scaled font still holds the unscaled font, is not modelled because scaled fonts throw. A null face or font face throws because Cairo dereferences it. An unreadable guest `face_index` also throws. Setting user data on a font face from its own finalizer, or destroying it there without a reference of the finalizer's own, throws because Cairo then modifies or frees what it is finalizing. The PS5 prototypes and response to an unreadable `face_index` are unverified.
