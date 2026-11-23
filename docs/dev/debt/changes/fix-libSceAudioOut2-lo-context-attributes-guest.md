# libSceAudioOut

## add Unknown function info

- [sceAudioOut2LoContextGetAttributes guest input](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Low.cpp) (libSceAudioOut) - each attribute is copied in with `GuestCopyIn_nid_no_patch` and the host `errno` preserved before its id is checked. An unreadable attribute returns `0x80268001` (`INVALID_PARAM`) instead of faulting the host; that fallback is unmeasured on a console. A readable attribute answers exactly as before.
