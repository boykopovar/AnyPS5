# libSceFontFt technical debt

### Functional

- [Font format support](../../../core/libs/prx/libSceFontFt/Export.cpp): format queries now take the font library, validate its magic and lock it while checking its actual FreeType modules. The host registers its default modules at creation rather than enabling formats individually, so format-selection state and guest allocator failures during format registration are not reproduced. Unsupported foreign drivers throw.
- [TrueType GX support](../../../core/libs/prx/libSceFontFt/Export.cpp): the GX query checks the bundled FreeType TrueType module, whose configuration enables GX variation support. The console's response for a FreeType build without GX variation support has not been measured.

### Unknown function info

- [Alias and glyph selection exports](../../../core/libs/prx/libSceFontFt/Export.cpp): alias initialization/termination take a handle pointer; alias registration takes a string and font ID or path. Glyph selection takes a 32-bit selector and returns a table pointer whose layout remains unresolved. Their protected implementations are unchanged.

- [sceFontSelectGlyphsFt and sceFontFtSupportSystemFonts](../../../core/libs/prx/libSceFontFt/Export.cpp) (libSceFontFt) - still throw. Read from the disassembly of the PS4 libSceFontFt.sprx on firmware 10.71, not from a PS5 module and not measured on a console. `sceFontSelectGlyphsFt` returns a static table with 0x0F1F at 0 and 0x0F03 at 4 for selector 0 and null otherwise. `sceFontFtSupportSystemFonts` is the same code as `sceFontFtSupportOpenType`; both pass `FTA_Support_Format_OpenType`.

- [sceFontFtSupportFontFormats](../../../core/libs/prx/libSceFontFt/Export.cpp) (libSceFontFt) - still throws. In the same module, it returns `INVALID_LIBRARY` unless the library magic is 0x0F01, then under the library lock calls `FTA_Support_Modules` with a mask built from the format bits: 0x100 for bit 0x80, then 0x4cc0 when all of 0x52 are set, 0x4c40 for 0x42, 0x4c80 for 0x50, 0xc80 for 0x10, 0x8c80 for 0x30, 0x1820 for 0x08, 0x1808 for 0x01 and 0x1c90 for 0x14. A result of 0 returns 0, 0x40 returns `ALLOCATION_FAILED` and any other `NO_SUPPORT_FORMAT`. What each mask bit selects in libSceFreeType was not read
