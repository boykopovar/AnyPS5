# libSceVdecCore.native

## change sceVdecCoreCreateDecoderBid, sceVdecCoreCreateDecoderSvp, sceVdecCoreMapMemory and sceVdecCoreSetDecodeInputSvp
- [sceVdecCoreCreateDecoderBid, sceVdecCoreCreateDecoderSvp, sceVdecCoreMapMemory and sceVdecCoreSetDecodeInputSvp](../../../core/libs/prx/libSceVdecCore.native/Export.cpp) (libSceVdecCore.native) - signatures and pre-decoder checks follow 13.60 `libSceVdecCore.native.sprx` (`0x480`, `0x1b90`, `0x23c0`, `0x23e0`, `0x2c30`); console execution is unmeasured. Rejections return `0x80C00001`, `0x80C00002`, `0x80C00007` or `0x80C00015`; calls passing validation still throw `NotImplemented_nid_no_patch`. Unreadable pointers fall back safely while preserving `errno`.
