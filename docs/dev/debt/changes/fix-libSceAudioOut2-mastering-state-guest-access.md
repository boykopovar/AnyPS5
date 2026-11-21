# libSceAudioOut

## add Mastering guest access

- [sceAudioOut2MasteringGetState guest access](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Mastering.cpp) (libSceAudioOut) - the state id is copied in and the whole state copied out through `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` with the host `errno` preserved. An unreadable or unwritable state returns `0x8026800C` instead of faulting the host; that fallback is unmeasured on a console. A readable state answers exactly as before, including the measured ids 1 and 2 and `0x80268204` for any other.
