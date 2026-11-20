# libSceAudioOut

## add Functional

- [sceAudioOut2ContextQueryMemory and sceAudioOut2ContextSetAttributes](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Context.cpp) (libSceAudioOut) - copying the context parameters and attribute records through `GuestCopyIn_nid_no_patch`, returning `SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT` while preserving `errno` for unmapped or unwritable guest pointers, is a host-safety fallback; the firmware behavior on invalid buffers has not been measured on a console.
