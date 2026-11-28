# libSceAudioOut

## add Functional

- [sceAudioOutOutputs](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - validating the parameter array length against `256` and copying the parameter records through `GuestCopyIn_nid_no_patch`, returning `-2144993276` while preserving `errno` for unmapped or oversized guest pointers, is a host-safety fallback; the firmware behavior on invalid buffers has not been measured on a console.
