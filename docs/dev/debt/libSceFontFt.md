# libSceFontFt technical debt

### Functional

- [Font format support](../../../core/libs/prx/libSceFontFt/Export.cpp): format queries now take the font library, validate its magic and lock it while checking its actual FreeType modules. The host registers its default modules at creation rather than enabling formats individually, so format-selection state and guest allocator failures during format registration are not reproduced. Unsupported foreign drivers throw.
- [TrueType GX support](../../../core/libs/prx/libSceFontFt/Export.cpp): the GX query checks the bundled FreeType TrueType module, whose configuration enables GX variation support. The console's response for a FreeType build without GX variation support has not been measured.

### Unknown function info

- [Alias and glyph selection exports](../../../core/libs/prx/libSceFontFt/Export.cpp): alias initialization/termination take a handle pointer; alias registration takes a string and font ID or path. Glyph selection takes a 32-bit selector and returns a table pointer whose layout remains unresolved. Their protected implementations are unchanged.
