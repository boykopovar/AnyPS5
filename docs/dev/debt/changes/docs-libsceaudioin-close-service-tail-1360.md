# libSceAudioIn

## add Functional

- [sceAudioInClose](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - measured on a PS5 Pro payload process (13.60): an invalid handle (-1) answers 0x80260101 locally, but a valid-base free slot (0x30000000) is not rejected by any local open-slot check - the call reaches the audio service and answers 0x8026010C with an `AINU Error: IPMI is not initialized in func sceAudioInClose()` line in this process without an initialized audio service (two identical sessions). What a title process answers for a free slot is unmeasured. AnyPS5 fails the free slot locally with INVALID_STATE (0x80260109) instead.
