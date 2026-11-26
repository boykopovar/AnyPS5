# libSceAudioIn

## change [sceAudioInIsPadEmbeddedMic](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn)

- [sceAudioInIsPadEmbeddedMic](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - returns 0 for every open port: on the console it returns 1 when the port captures from the controller's built-in microphone and 0 for another input device, but ports here capture from the host's default input device. The export is absent from the 4.03 module; measured on a PS5 Pro payload process (13.60) with no port open: every handle answers 0x80260109 - including -1, out-of-table index 8 and the valid-base free slot - the state-only rejection of sceAudioInIsSharedDevice rather than the bit-checked gate sceAudioInGetSilentState uses there (-1 and index 8 answer 0x80260101 there, a free slot 0x80260109); this implementation used to answer INVALID_HANDLE for such handles and now answers INVALID_STATE like the console.
