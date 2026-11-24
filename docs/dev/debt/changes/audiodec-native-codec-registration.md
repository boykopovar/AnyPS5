# libSceAudiodec.native

## change [libSceAudiodec.native pending exports]

- [libSceAudiodec.native pending exports](../../../core/libs/prx/libSceAudiodec.native/Export.cpp) - 13 exports have signatures and argument checks from @BunnyFl's [firmware 13.42 disassembly](https://github.com/boykopovar/AnyPS5/issues/19399): `sceAudiodecInitialize`, `sceAudiodecTerminate`, `sceAudiodecGetContext`, `sceAudiodecCreateDecoderEx`, `sceAudiodecDeleteDecoderEx`, `sceAudiodecClearContextEx`, and the seven decode variants (`Decode2`, `Decode2Ex`, `Decode2WithPriority`, `Decode2WithPriorityEx`, `DecodeEx`, `DecodeWithPriority`, `DecodeWithPriorityEx`). Still unimplemented: Ajmi and Ajm service calls (`QhA7rTjJbro`, `BD002VBfhuE`, `sceAjmInitialize`, `sceAjmFinalize`, `sceAjmInstanceDestroy`), and per-codec routines (`0xaa0`, `0x16b0`, `0x1060`, `0x3100`, `0x3410`, `0x1ba0`, `0x22a0`, `0x28d0`, `0x1870`, `0x1370`, `0xd50`, `0x3710`, `0x1e80`, `0x2490`, `0x2da0`).

## remove [sceAudiodecUnregisterCodec]

## add Functional

- [sceAudiodecRegisterCodec and sceAudiodecUnregisterCodec](../../../core/libs/prx/libSceAudiodec.native/Export.cpp) (libSceAudiodec.native) - dispatch to Ajm/Ajmi service calls for types 1, 2, 3 (Ajm codecs 1, 0, 2), 4, 6, 10 (Ajmi codecs 6, 9, 20), and 11, 12 (Ajm codec 24 under the write lock with state flags) following firmware 13.42 disassembly and execution traces with busy retry on 0x80931007 after 1,000 microseconds and 0x80930008 mapped to 0x807F0001 on registration; physical console behavior is unmeasured.
