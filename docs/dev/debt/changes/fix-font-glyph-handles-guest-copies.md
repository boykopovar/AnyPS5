# libSceFont

## add Functional

- [Generated glyph handles](../../../core/libs/prx/libSceFont/src/Render.cpp) (libSceFont) - `sceFontGenerateCharGlyph` writes the handle and reads the generate detail and its memory record's magic through checked copies, and `sceFontDeleteGlyph` reads and clears the handle the same way. An unreadable or unwritable handle, detail or memory record returns `SCE_FONT_ERROR_INVALID_PARAMETER` before a glyph is created or freed; the console's result for it is not measured.
