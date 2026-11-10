# libSceFontFt technical debt

### Functional

- [Font format support](../../../core/libs/prx/libSceFontFt/Export.cpp): format queries now take the font library, validate its magic and lock it while checking its actual FreeType modules. The host registers its default modules at creation rather than enabling formats individually, so format-selection state and guest allocator failures during format registration are not reproduced. Unsupported foreign drivers throw.
- [TrueType GX support](../../../core/libs/prx/libSceFontFt/Export.cpp): the GX query checks the bundled FreeType TrueType module, whose configuration enables GX variation support. The console's response for a FreeType build without GX variation support has not been measured.
- [System font format support](../../../core/libs/prx/libSceFontFt/Export.cpp): `sceFontFtSupportSystemFonts` queries the bundled FreeType CFF and TrueType modules for OpenType font support, matching `sceFontFtSupportOpenType`. Console behavior without OpenType support has not been measured.

### Unknown function info

- [Alias exports](../../../core/libs/prx/libSceFontFt/Export.cpp): alias initialization/termination take a handle pointer; alias registration takes a string and font ID or path. Their protected implementations are unchanged.

- [sceFontFtSupportSystemFonts](../../../core/libs/prx/libSceFontFt/Export.cpp) (libSceFontFt) - still throws. Read from the disassembly of the PS4 libSceFontFt.sprx on firmware 10.71, not from a PS5 module and not measured on a console. sceFontFtSupportSystemFonts is the same code as sceFontFtSupportOpenType; both pass FTA_Support_Format_OpenType. sceFontSelectGlyphsFt returns a static table with 0x0F1F at 0 and 0x0F03 at 4 for selector 0 and null otherwise.
- [sceFontSelectGlyphsFt](../../../core/libs/prx/libSceFontFt/Export.cpp) (libSceFontFt) - implemented and verified against the PS5 4.03 module. `sceFontSelectGlyphsFt` returns the static 8-byte record `0x0F1F, 0xF200, 0x0F03, 0xF8F0` (u16s) for selector 0 and null otherwise; what the 0xF200 and 0xF8F0 fields mean was not read.
- [sceFontFtSupportFontFormats](../../../core/libs/prx/libSceFontFt/Export.cpp) (libSceFontFt) - queries the bundled FreeType modules corresponding to the requested format bitmask (0x01/0x02/0x40/0x42/0x52/0x80 for TrueType, 0x10/0x40/0x50/0x52 for CFF, 0x20 for Type 1, 0x04 for Type 42, and 0x08 for CID), returning `SCE_FONT_ERROR_NO_SUPPORT_FORMAT` for unrecognised bit positions outside 0xFF. Guest allocator failures during format registration are not reproduced.
