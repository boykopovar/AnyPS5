# libSceAudioOut

## add Unknown function info

- [sceAudioOut2SpeakerArrayCreate and sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients guest pointers](../../../core/libs/prx/libSceAudioOut/src/AudioOut2SpeakerArray.cpp) (libSceAudioOut) - the VBAP parameters and speaker positions are copied in, the array is built on the host and copied into the caller's memory, and the handle and coefficients are copied out, with `errno` preserved; an unreadable or unwritable pointer throws the invalid-argument exception the null pointers already throw, because whether the console faults or returns an error on unmapped memory is unmeasured. The created array is still read in place by handle.
