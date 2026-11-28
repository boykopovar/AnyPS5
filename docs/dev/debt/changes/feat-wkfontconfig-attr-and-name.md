# libSceWkFontConfig

## remove [libSceWkFontConfig font lookups]

## add Functional

- [sceWkFontConfigGetAttr, sceWkFontConfigGetFont, sceWkFontConfigGetFontFromFontName, sceWkFontConfigGetFontName and sceWkFontConfigGetFontPath](../../../core/libs/prx/libSceWkFontConfig/Export.cpp) - inaccessible guest memory throws where the console module faults. The attribute offset and copy order follow the [firmware 13.42 disassembly](https://github.com/boykopovar/AnyPS5/pull/18722) at `0x61f` through `0x62c`. The final four bytes are copied before the first sixteen. `sceWkFontConfigGetFontFromFontName` reads the requested name one byte at a time, only as far as the module's `strcmp` calls would, so an unreadable byte past the first mismatch does not throw.
- [sceWkFontConfigGetFontPath](../../../core/libs/prx/libSceWkFontConfig/Export.cpp) - returns `/`, `sceKernelGetFsSandboxRandomWord()` and the record's path, as `libSceWkFontConfig.sprx` (firmware 13.40) does, but no path provider or alias in AnyPS5 maps `/<word>/common/font` or `/<word>/common/font2` to font files, so the returned paths do not open.

## add Unknown function info

- [The font table and selection](../../../core/libs/prx/libSceWkFontConfig/Export.cpp) - read from the disassembly and data of `libSceWkFontConfig.sprx` (firmware 13.40), not run on a console; what the font IDs and attribute words mean is unknown, and the field names in the code are guesses from the table values. The 17 0x28-byte records and seven lists are rebuilt from the module's data. `sceWkFontConfigGetFont` maps IDs 0, 20, 22, 55 and 108 to Japanese, 1 and 2 to Arabic, 8, 14 and 25 to SST (Typewriter when word 1 is 3, Italic when word 3 is 1), 18 and 122 to Hangul, 17 to Japanese unless `sceSystemServiceParamGetInt(1)` first gave 9 to 11, and other IDs to DFHei. In a list it takes the matching weight (word 2, 4 when 0; 5 also matches 4), else the last lighter record, else the first. `sceWkFontConfigGetFontFromFontName` searches SST, Italic, Typewriter, Japanese, Arabic, DFHei, Hangul and returns `0x813C0003` on no match.
