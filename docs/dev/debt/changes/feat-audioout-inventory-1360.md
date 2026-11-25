# libSceAudioOut

## add Functional

- [sceAudioOut2GetSystemDebugState, sceAudioOut2SetHrtfIdForCronos, sceAudioOut2AssignHRTF, and sceAudioOut2SetTvCorrectionInfo](../../../core/libs/prx/libSceAudioOut/src/InventoryExports.cpp) - Verified against PS5 firmware 13.60 disassembly: `sceAudioOut2GetSystemDebugState` returns `0x80268006` before AudioOut2 initialization; `sceAudioOut2SetHrtfIdForCronos` and `sceAudioOut2AssignHRTF` validate `hrtfId` (`-1`, `0x80`, or `1..5`, else `0x80268001`) before `userId` (`0xfe`, `0xff`, `-1` return `0x80268010`); `sceAudioOut2SetTvCorrectionInfo` returns `0x8026800c` for a null info pointer before `0x80268001` for `mode > 1`. Past these gates, audio-server shared memory and IPC remain unimplemented and throw via `NotImplemented_nid_no_patch`.
