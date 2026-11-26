# libSceVoice

## add Functional

- [sceVoiceGetPortAttr](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - returning InvalidArgument for an unwritable attribute destination pointer is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
