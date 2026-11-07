# libSceFontFt technical debt

### Functional

- [Font format support](../../../core/libs/prx/libSceFontFt/Export.cpp): format queries now take the font library, validate its magic and lock it while checking its actual FreeType modules. The host registers its default modules at creation rather than enabling formats individually, so format-selection state and guest allocator failures during format registration are not reproduced. Unsupported foreign drivers throw.
- [TrueType GX support](../../../core/libs/prx/libSceFontFt/Export.cpp): the GX query checks the bundled FreeType TrueType module, whose configuration enables GX variation support. The console's response for a FreeType build without GX variation support has not been measured.
- [System font format support](../../../core/libs/prx/libSceFontFt/Export.cpp): `sceFontFtSupportSystemFonts` queries the bundled FreeType CFF and TrueType modules for OpenType font support, matching `sceFontFtSupportOpenType`. Console behavior without OpenType support has not been measured.

### Unknown function info

- [Alias and glyph selection exports](../../../core/libs/prx/libSceFontFt/Export.cpp): alias initialization/termination take a handle pointer; alias registration takes a string and font ID or path. Glyph selection takes a 32-bit selector and returns a table pointer whose layout remains unresolved. Their protected implementations are unchanged.

- [sceFontSelectGlyphsFt and sceFontFtSupportSystemFonts](../../../core/libs/prx/libSceFontFt/Export.cpp) (libSceFontFt) - still throw. Read from the disassembly of the PS4 libSceFontFt.sprx on firmware 10.71, not from a PS5 module and not measured on a console. `sceFontSelectGlyphsFt` returns a static table with 0x0F1F at 0 and 0x0F03 at 4 for selector 0 and null otherwise. `sceFontFtSupportSystemFonts` is the same code as `sceFontFtSupportOpenType`; both pass `FTA_Support_Format_OpenType`.

- [sceFontFtSupportFontFormats](../../../core/libs/prx/libSceFontFt/Export.cpp) (libSceFontFt) - queries the bundled FreeType modules corresponding to the requested format bitmask (0x01/0x02/0x40/0x42/0x52/0x80 for TrueType, 0x10/0x40/0x50/0x52 for CFF, 0x20 for Type 1, 0x04 for Type 42, and 0x08 for CID), returning `SCE_FONT_ERROR_NO_SUPPORT_FORMAT` for unrecognised bit positions outside 0xFF. Guest allocator failures during format registration are not reproduced.
