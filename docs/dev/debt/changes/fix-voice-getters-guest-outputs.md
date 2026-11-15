# libSceVoice

## add Functional

- [sceVoiceGetVolume, sceVoiceGetPortInfo and sceVoiceGetMemorySize](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - returning InvalidArgument with caller errno preserved when output pointers fail ReadGuest or WriteGuest is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
