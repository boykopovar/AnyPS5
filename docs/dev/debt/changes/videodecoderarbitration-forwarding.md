# libSceVideoDecoderArbitration

## change [sceVideoDecoderArbitrationAcceptEvent, sceVideoDecoderArbitrationEnable and sceVideoDecoderArbitrationInitialize]

- [sceVideoDecoderArbitrationAcceptEvent, sceVideoDecoderArbitrationEnable and sceVideoDecoderArbitrationInitialize](../../../core/libs/prx/libSceVideoDecoderArbitration/Export.cpp) (libSceVideoDecoderArbitration) - forward to `libSceVideoArbitration`'s `sceVideoArbitrationAcceptEvent`, `sceVideoArbitrationEnable` and `sceVideoArbitrationInitialize` and map arbitration results to decoder arbitration error codes. Parameter block safe copies preserve `errno` and return `0x81570001` on unreadable memory.

## change [The argument checks]

- [The argument checks](../../../core/libs/prx/libSceVideoDecoderArbitration/Export.cpp) (libSceVideoDecoderArbitration) - in firmware 13.42's `libSceVideoDecoderArbitration.sprx` (SHA-256 `40ddb3e99ee78bacf0196519ab82658140cec80ea8d6bc734124c66bb954eeb4`), read from disassembly and not run on a console: `sceVideoDecoderArbitrationInitialize` (`0xf0`) returns `0x81570001` for a null block or a 64-bit size other than 0x18, `0x81570002` for an initialized module (`0xc018`), `0x81570004` for a 32-bit value at offset 8 outside 0x100-0x2FF, and `0x81570001` for a 64-bit value at offset 0x10 outside 1-0x7F; `sceVideoDecoderArbitrationEnable` (`0x1e0`) returns `0x81570001` for a nonzero first or a null second argument and `0x81570002` if already enabled; `sceVideoDecoderArbitrationAcceptEvent` (`0x2a0`) returns `0x81570001` for an event above 1.
