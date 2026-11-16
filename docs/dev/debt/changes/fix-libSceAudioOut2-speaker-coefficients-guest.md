# libSceAudioOut

## add Unknown function info

- [sceAudioOut2GetSpeakerArrayCoefficients guest output](../../../core/libs/prx/libSceAudioOut/src/AudioOut2SpeakerArray.cpp) (libSceAudioOut) - the coefficients are built on the host and written with the file's `WriteGuest` helper; an unwritable output throws the existing invalid-argument path after writing nothing, instead of faulting the host. That fallback is unmeasured on a console, and a writable output answers exactly as before.
