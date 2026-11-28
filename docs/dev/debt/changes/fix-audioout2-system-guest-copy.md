# libSceAudioOut

## add Functional

- [sceAudioOut2SetSystemDebugState guest input](../../../core/libs/prx/libSceAudioOut/src/AudioOut2System.cpp) (libSceAudioOut) - copies `AudioOut2SystemDebugStateParam` through `GuestCopyIn_nid_no_patch` while preserving host `errno`, returning `0x8026800C` on unreadable pointers instead of dereferencing guest memory directly; console behavior on unmapped memory is unmeasured.
