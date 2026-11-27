# libSceAudiodec.native

## remove [sceAudiodecUnregisterCodec]

## add Functional

- [sceAudiodecRegisterCodec and sceAudiodecUnregisterCodec](../../../core/libs/prx/libSceAudiodec.native/Export.cpp) - dispatch to Ajm/Ajmi service calls for types 1-3 (Ajm 1, 0, 2), 4, 6, 10 (Ajmi 6, 9, 20), and 11-12 (Ajm 24 under guest rwlock with state flags), per @BunnyFl's [13.42 run](https://github.com/boykopovar/AnyPS5/issues/13680#issuecomment-6537745276) and [disassembly](https://github.com/boykopovar/AnyPS5/issues/19399) (only `libSceAudiodec.native` exports `sceAudiodecUnregisterCodec`, per [13.60](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceaudiodec-2dbab67864.jsonl)). Busy retry on `0x80931007` sleeps 1000 us up to `MaxRetries = 1000` (unbounded on console); `0x80930008` maps to `0x807F0001`. For types 11/12, flags are checked before `GuestWritable_nid_no_patch` and re-read under `scePthreadRwlockWrlock` (nonzero returns `0x807F0002`); console behavior is unmeasured.
