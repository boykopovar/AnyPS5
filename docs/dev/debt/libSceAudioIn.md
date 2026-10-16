# libSceAudioIn technical debt

### Silent stubs

- [sceAudioInInit](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - returns 0 without library-wide setup, as `sceAudioOutInit` does; each `sceAudioInOpen` opens its own SDL capture device

### Unknown function info

- [sceAudioInIsPadEmbeddedMic](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - name from its NID (`X+4jdIS75P0`); signature `int32_t(int32_t handle)` from the Ghost of Yotei (PPSA26344) call site in #2355; the result's meaning is unconfirmed, so it still throws rather than invent one

### Functional

- [sceAudioInIsPadEmbeddedMic](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) returns `0` for every valid input handle because the host capture path exposes no separate pad-embedded microphone. An invalid handle returns `AUDIO_IN_ERROR_INVALID_HANDLE`

- [sceAudioInHqOpen, sceAudioInAsyncOpen](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) open the same port as `sceAudioInOpen`, so `sceAudioInInput`, `sceAudioInGetSilentState` and `sceAudioInClose` accept their handles. The signatures agree across [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/fc5d2cc219c43c8c8f354153e7b3d0c04a5d506e/src/core/libraries/audio/audioin.cpp), [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/60977b29103452b9681ae10c7a9011d44965bf16/src/SharpProspero/Interop/Audio/AudioIn.cs) and [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/c86c8b4d9333a766cc8c4495f89f893bf35e6199/src/libs/audio.cpp). HqOpen accepts the same validated sizes and frequencies as the regular open, and unsupported format `0x10` returns `AUDIO_IN_ERROR_INVALID_PARAM`; AsyncOpen uses the same backend for sizes from 1 through 384 after type, index and frequency validation. Host capture pacing and console behavior remain unverified.
