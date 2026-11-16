# libSceAudioOut

## add Unknown function info

- [sceAudioOut2SetSystemDebugState guest access](../../../core/libs/prx/libSceAudioOut/src/AudioOut2System.cpp) (libSceAudioOut) - the 0x18-byte parameter block is copied in with `GuestCopyIn_nid_no_patch` and the host `errno` preserved. An unreadable block returns `0x8026800C` instead of faulting the host; that fallback is unmeasured on a console. The inner `param` pointer is still only checked for null, as measured, and a readable block answers exactly as before.
