# libSceNgs2.native

## add Functional

- [sceNgs2VoiceRunCommands guest pointers](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) (libSceNgs2.native) - command records and matrix level arrays are copied through `GuestCopyIn_nid_no_patch` / `ReadGuestBytes` while preserving caller `errno`; returning `SCE_NGS2_ERROR_INVALID_OUT_ADDRESS` (`0x804A8010`) for unreadable `commands` or `levels` pointers is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
