# libSceTextToSpeech

## change [sceTextToSpeechInitImpl]

- [sceTextToSpeechInitImpl](../../../core/libs/prx/libSceTextToSpeech/Export.cpp) (libSceTextToSpeech) - answers `0x814900FF` (`TextToSpeechServerUnreachable`) when called with a valid id and non-null parameter without a text-to-speech server, for all process types without restricting to PS4 callers.
