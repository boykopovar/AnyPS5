# libSceVoice

## add Functional

- [sceVoiceInit guest parameters](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - returning SCE_VOICE_ERROR_INVALID_ARGUMENT for an unreadable initParam pointer via GuestCopyIn_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
