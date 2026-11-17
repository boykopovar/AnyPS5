# libSceVoice

## add Functional

- [sceVoiceGetPortInfo guest output](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - returning SCE_VOICE_ERROR_INVALID_ARGUMENT for an unwritable portInfo pointer via GuestCopyOut_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
