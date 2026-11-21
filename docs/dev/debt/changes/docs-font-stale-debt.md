# libSceFont

## remove [sceFontGraphicsReportError]

## add Unknown function info

- [sceFontGraphicsReportError](../../../core/libs/prx/libSceFont/src/Unimplemented.cpp) - follows the PS4 10.71 `libSceFont.sprx` body (offset 0x1cf20): passes the error to the callback set by `sceFontSetGraphicsErrorCallback` and returns 0, or returns 0x80460020 when none is set. Not checked on a console or against a PS5 module.

## change is byte-exact against the PS5 4.03 module

- [sceFontOutlineVertexesInterfaceInit](../../../core/libs/prx/libSceFont/src/Unimplemented.cpp) and [sceFontSupportSystemUpdation](../../../core/libs/prx/libSceFont/src/Library.cpp) (libSceFont) - implemented. `sceFontOutlineVertexesInterfaceInit` is byte-exact against the PS5 4.03 module (`guest_font` pins the record). `sceFontSupportSystemUpdation` follows the PS4 10.71 disassembly that BunnyFl confirmed in the PS5 13.42 module ([review](https://github.com/boykopovar/AnyPS5/pull/14879#pullrequestreview-5736256677)); the export is absent from 4.03: `INVALID_LIBRARY` unless the library magic is 0x0F01, and under the lock version 0x20230801 sets bit 1 of byte 3 and returns 0, else `INVALID_PARAMETER`. What that bit changes was not read

## change implement the contracts read from PS4 10.71

- [sceFontOutlineVertexesInterfaceInit](../../../core/libs/prx/libSceFont/src/Unimplemented.cpp) and [sceFontSupportSystemUpdation](../../../core/libs/prx/libSceFont/src/Library.cpp) implement the contracts read from PS4 10.71 libSceFont.sprx and independently confirmed in PS5 13.42 by [BunnyFl](https://github.com/boykopovar/AnyPS5/pull/14879#pullrequestreview-5736256677). The outline initializer clears its 48-byte interface, accepts a non-null first pointer and a 64-bit count from 1 to 3, and preserves the second pointer even when null. SystemUpdation validates and locks the library, then sets bit 1 of byte 3 only for version 0x20230801. What that flag changes in later operations remains unrecovered; no title rendering or PS5 console measurement is claimed.
