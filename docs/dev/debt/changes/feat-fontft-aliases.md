# libSceFontFt

## remove [Alias exports]

## add Functional

- [Alias table](../../../core/libs/prx/libSceFontFt/Export.cpp) (libSceFontFt) - the four alias exports follow PS4's `libSceFontFt.sprx` (SHA-256 `ad78707187be911df1559f4799fef2af3d6c4d098dd95b33d3e22854cca15e2b`, `0x11680`-`0x11a1a`, read from disassembly, not run on a console). They share one 0x2020-byte table (magic 0x1fa0 at 0, count at 4, index top at 8, string cursor at 0x10, free end at 0x18). Each alias appends its strings at the cursor and a 16-byte index record below the top. As in the module, `sceFontFtSetAliasPath` does not lower the free end, so later aliases can overwrite index records, and failed calls leave the bytes they copied. `sceFontFtSetAliasFont` asks `sceFontControl` (command `0x46fa00`) to write the font's path, so it throws there until libSceFont implements that command. Writes outside the table and unreadable strings throw where the module would corrupt memory or fault. Nothing reads the table yet.
