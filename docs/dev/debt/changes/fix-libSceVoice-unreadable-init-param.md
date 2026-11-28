# libSceVoice

## add Functional

- [sceVoiceInit guest parameters](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - reading `param->app_type` (4 bytes) via `GuestCopyIn_nid_no_patch` and returning `SCE_VOICE_ERROR_INVALID_ARGUMENT` while restoring caller `errno` when `param` is unreadable is a host-safety fallback; whether a PS5 faults or returns an argument error on unmapped parameter memory is unmeasured. Closes when: measured on hardware. Labels: console, tested.
