# libSceAudioOut

## add Unknown function info

- [sceAudioOutSetVolume](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - copies in channel volume settings via GuestCopyIn_nid_no_patch with caller errno preserved; returning AUDIO_OUT_ERROR_INVALID_POINTER (-2144993276) for an unmapped or unreadable pointer is a host-safety fallback with console behavior unmeasured.
