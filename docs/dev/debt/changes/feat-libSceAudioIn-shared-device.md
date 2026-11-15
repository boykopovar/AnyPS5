# libSceAudioIn

## change [sceAudioInIsSharedDevice]

- [sceAudioInIsSharedDevice](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - 4.03 checks only the low-byte index (<=7) and the slot's open flag; unlike the other handle gates it skips the 0x30000000 high-bits test. The host returns 0 (not a shared device) for every open port, matching the default host audio capture device behavior in `sceAudioInIsPadEmbeddedMic`.
