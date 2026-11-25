# libSceVdecCore.native

## change sceVdecCoreCreateDecoderBid

- [sceVdecCoreCreateDecoderBid, sceVdecCoreCreateDecoderSvp, sceVdecCoreMapMemory and sceVdecCoreSetDecodeInputSvp](../../../core/libs/prx/libSceVdecCore.native/Export.cpp) (libSceVdecCore.native) - follow firmware 13.60 `libSceVdecCore.native.sprx` disassembly, not measured on a console. `sceVdecCoreCreateDecoderBid` and `sceVdecCoreCreateDecoderSvp` delegate decoder creation to `sceVdecCoreCreateDecoder`; `sceVdecCoreMapMemory` enforces the 16-call `0x80C00015` cap and maps one `VdecCoreMemoryBlock`; `sceVdecCoreSetDecodeInputSvp` checks 64-byte alignment of `auData` (`0x80C00007`) and delegates to `sceVdecCoreSetDecodeInput`. Hardware BID and SVP memory isolation are not modelled.
