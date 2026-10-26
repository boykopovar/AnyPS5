# libSceTextToSpeech2 technical debt

### Silent stubs

- [libSceTextToSpeech2](../../../core/libs/prx/libSceTextToSpeech2/Export.cpp) has no speech engine: `sceTextToSpeech2Initialize` and the other exports return `EOPNOTSUPP`. PPSA01317 skips its narration set-up when initialization fails

### Unknown function info

- [sceTextToSpeech2Initialize](../../../core/libs/prx/libSceTextToSpeech2/Export.cpp) (libSceTextToSpeech2) - the console's error for an unavailable speech engine is unknown; it returns the kernel's `EOPNOTSUPP` (0x8002002D). PPSA01317 passes a param that starts with 0x2000000 and only tests the result for non-zero.
