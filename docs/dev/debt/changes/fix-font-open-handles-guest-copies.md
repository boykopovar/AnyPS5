# libSceFont

## add Functional

- [Font open handles](../../../core/libs/prx/libSceFont/src/Open.cpp) (libSceFont) - `sceFontOpenFontMemory`, `sceFontOpenFontFile`, `sceFontOpenFontInstance` and `sceFontOpenFontSet` read the caller's handle (the reuse hint) and write the result through checked copies, and read `FontOpenDetail` the same way. An unreadable or unwritable handle, or an unreadable detail, returns `SCE_FONT_ERROR_INVALID_PARAMETER`; the console's result for it is not measured. The font bytes `sceFontOpenFontMemory` is given are still read in place by the driver.
