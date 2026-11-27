# libSceTextToSpeech2

## change sceTextToSpeech2ConvertTextInt

- [sceTextToSpeech2ConvertTextInt, sceTextToSpeech2GetAudioInt, sceTextToSpeech2InitializeInt, sceTextToSpeech2OpenInt, sceTextToSpeech2RegisterTextConversionItem and sceTextToSpeech2SynthesizeInt](../../../core/libs/prx/libSceTextToSpeech2/Export.cpp) retain parameterless placeholder declarations in Api.hpp without confirmed ABI signatures. sceTextToSpeech2InitializeInt returns EOPNOTSUPP (0x8002002D), while the other five exports return SCE_TEXT_TO_SPEECH2_ERROR_NOT_INITIALIZED (0x81A10004) when the speech engine is uninitialized, matching libSceTextToSpeech2.sprx (0x6a0, 0x920, 0x980, 0x9e0, 0xa40); initialized-engine execution paths remain unimplemented.
