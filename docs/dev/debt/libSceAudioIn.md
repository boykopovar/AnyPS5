# libSceAudioIn technical debt

### Silent stubs

- [sceAudioInInit](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - returns 0 without library-wide setup, as `sceAudioOutInit` does; each `sceAudioInOpen` opens its own SDL capture device

### Unknown function info

- [sceAudioInIsPadEmbeddedMic](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - name from its NID (`X+4jdIS75P0`); signature `int32_t(int32_t handle)` from the Ghost of Yotei (PPSA26344) call site in #2355; the result's meaning is unconfirmed, so it still throws rather than invent one

### Functional

- [sceAudioInHqOpen, sceAudioInAsyncOpen](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) open the same port as `sceAudioInOpen`, so `sceAudioInInput`, `sceAudioInGetSilentState` and `sceAudioInClose` accept their handles. The signatures agree across [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/fc5d2cc219c43c8c8f354153e7b3d0c04a5d506e/src/core/libraries/audio/audioin.cpp), [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/60977b29103452b9681ae10c7a9011d44965bf16/src/SharpProspero/Interop/Audio/AudioIn.cs) and [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/c86c8b4d9333a766cc8c4495f89f893bf35e6199/src/libs/audio.cpp). HqOpen is limited to 128 samples at 48 kHz, as [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/c86c8b4d9333a766cc8c4495f89f893bf35e6199/src/libs/audio.cpp) and [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/60977b29103452b9681ae10c7a9011d44965bf16/src/SharpProspero/Interop/Audio/AudioIn.cs) describe the higher-quality path: a longer grain returns the invalid-size error and a lower rate the invalid-frequency error. AsyncOpen also accepts the 384-sample grain, the largest [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/60977b29103452b9681ae10c7a9011d44965bf16/src/SharpProspero/Interop/Audio/AudioIn.cs) documents for the low-latency path, and returns the invalid-size error for every other size; the console's shorter average return interval of an async port's blocks is not modelled, so like a synchronous port `sceAudioInInput` paces one grain at a time. Param 0x10 opens a float mono port like 0x11, as [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/c86c8b4d9333a766cc8c4495f89f893bf35e6199/src/libs/audio.cpp) accepts both, while SharpProspero documents only 0x11. Host capture pacing and console behavior remain unverified.
