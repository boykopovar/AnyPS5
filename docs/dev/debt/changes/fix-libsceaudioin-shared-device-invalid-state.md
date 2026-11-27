# libSceAudioIn

## change [sceAudioInIsSharedDevice](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn)

- [sceAudioInIsSharedDevice](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - 4.03 checks only the low-byte index (<=7) and the slot's open flag; unlike the other handle gates it skips the 0x30000000 high-bits test. Measured on a PS5 Pro payload process (13.60) with no port open: every handle answers 0x80260109 - including out-of-range indices (-1, 7, 8, 0x30000007, 0x45670000) - so the rejection path shows neither an index gate nor a high-bits gate; this implementation answered INVALID_HANDLE for out-of-range indices and now answers INVALID_STATE like the console. The shared-state answer is a shm service field (returns shm+0x402c == 3), so the export throws past the checks; with no port open none of that state is reachable from a payload process.
