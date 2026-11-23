# libSceAudioOut

## add Functional

- [sceAudioOut2GetSpeakerArrayCoefficients](../../../core/libs/prx/libSceAudioOut/src/AudioOut2SpeakerArray.cpp) (libSceAudioOut) - copies the speaker-array gain coefficients to the caller through `GuestCopyOut_nid_no_patch` while preserving host `errno`; rejecting unwritable non-null outputs with `APS5_INVALID_ARG_EX` is a host-safety fallback with console behavior unmeasured.
