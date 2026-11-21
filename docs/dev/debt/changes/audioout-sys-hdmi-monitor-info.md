# libSceAudioOut

## add Unknown function info

- [sceAudioOutSysGetHdmiMonitorInfo guest output](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - the empty monitor info is written with `GuestCopyOut_nid_no_patch` and `errno` preserved; returning `0x80260004` (the invalid-pointer error it returns for a null info) for an unwritable info is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
