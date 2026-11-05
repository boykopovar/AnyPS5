# libSceTextToSpeech2 technical debt

### Silent stubs

- [libSceTextToSpeech2](../../../core/libs/prx/libSceTextToSpeech2/Export.cpp) has no speech engine: `sceTextToSpeech2Initialize`, `sceTextToSpeech2Open`, `sceTextToSpeech2Speak`, `sceTextToSpeech2Cancel`, `sceTextToSpeech2Close`, `sceTextToSpeech2Terminate` and the system/speech status queries return `EOPNOTSUPP`. PPSA01317 skips its narration set-up when initialization fails

### Unknown function info

- [sceTextToSpeech2ConvertTextInt, sceTextToSpeech2GetAudioInt, sceTextToSpeech2InitializeInt, sceTextToSpeech2OpenInt, sceTextToSpeech2RegisterTextConversionItem and sceTextToSpeech2SynthesizeInt](../../../core/libs/prx/libSceTextToSpeech2/Export.cpp) have unknown signatures and behaviour. The parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures, and every call throws through `NotImplemented_nid_no_patch`. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscetexttospeech2-b43e9ee18c.jsonl); not measured on a console.
- [sceTextToSpeech2Initialize](../../../core/libs/prx/libSceTextToSpeech2/Export.cpp) (libSceTextToSpeech2) - the console's error for an unavailable speech engine is unknown; it returns the kernel's `EOPNOTSUPP` (0x8002002D). PPSA01317 passes a param that starts with 0x2000000 and only tests the result for non-zero.
