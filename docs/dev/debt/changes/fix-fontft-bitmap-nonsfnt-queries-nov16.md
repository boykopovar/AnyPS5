# libSceFontFt

## add Functional

- [Non-SFNT font queries](../../../core/libs/prx/libSceFontFt/src/Driver.cpp): Bitmap BDF faces can be opened by the bundled FreeType driver but have no SFNT `head` or `maxp` tables. Style queries now use face attributes without a `head` direction bit, and outline-profile queries return `SCE_FONT_ERROR_NO_SUPPORT_FUNCTION` without a supported `maxp`; both are regression-tested through `sceFontOpenFontMemory` on Windows. The console's results for these queries on bitmap fonts have not been measured.
