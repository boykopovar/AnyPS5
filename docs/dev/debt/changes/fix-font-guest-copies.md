# libSceFont

## add Functional

- [Style frame records](../../../core/libs/prx/libSceFont/src/Style.cpp) (libSceFont) - every `sceFontStyleFrame*` call reads the caller's frame through a checked copy, `sceFontStyleFrameInit` writes the whole frame in one copy, and the other setters write only the fields they change. An unreadable or unwritable frame returns `SCE_FONT_ERROR_INVALID_PARAMETER` with `errno` preserved; the console's result for an unmapped frame is not measured.
- [Font memory records and library handles](../../../core/libs/prx/libSceFont/src/Library.cpp) (libSceFont) - `sceFontMemoryInit` and `sceFontMemoryTerm` read and write the caller's `FontMemory` through checked copies, and `sceFontDestroyLibrary` and `sceFontDestroyRenderer` read and clear the caller's handle the same way. An unreadable or unwritable record or handle returns `SCE_FONT_ERROR_INVALID_PARAMETER` before anything is released. The memory interface table that `sceFontMemoryTerm` calls through is still read in place. A host-safety fallback, not measured on the console.

## change [sceFontGetFontMetrics]

- [sceFontGetFontMetrics](../../../core/libs/prx/libSceFont/src/Layout.cpp) (libSceFont) - the 64-byte output, the form fallbacks (2 and 3 fall back to the `hhea` values when OS/2 is missing or gives an empty range, 4 builds vertical values from `vhea` or OS/2) and the raw weight deltas and slant copied from the style are not measured on a console. For a font set the values are those of its primary file; which file the console reports is not recovered. The output is read and written back through checked copies, so fields a form doesn't set keep their bytes, and an unreadable or unwritable output returns `SCE_FONT_ERROR_INVALID_PARAMETER`. Closes when: measured on a PS5

## change [sceFontGetFontStyleInformation]

- [sceFontGetFontStyleInformation](../../../core/libs/prx/libSceFont/src/Layout.cpp) (libSceFont) - the 24-byte output (FreeType family and style names, italic and bold flags, OS/2 weight class when it is a multiple of 100 from 100 to 900, width class below 10, and 1 plus a right-to-left `head` direction plus 0x80 for vertical metrics) and the font-set record path are not measured on a console. A face without a `head` table, such as a BDF font, throws, as its console result is unknown. The output is read and written back through checked copies, so fields a form doesn't set keep their bytes, and an unreadable or unwritable output returns `SCE_FONT_ERROR_INVALID_PARAMETER`. Closes when: measured on a PS5
