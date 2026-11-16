# libSceAudioOut

## add Unknown function info

- [sceAudioOut2PortSetAttributes guest access](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Port.cpp) (libSceAudioOut) - the attribute list, the data pointer it carries and the volume payload are read through `GuestCopyIn_nid_no_patch` with the host `errno` preserved. An unreadable list or payload returns `0x80260502` (`INVALID_ARGUMENT`) instead of faulting the host; that fallback is unmeasured on a console. Valid attributes behave exactly as before.
