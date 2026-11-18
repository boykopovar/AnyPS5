# libSceAudioOut

## add Functional

- [sceAudioOut2LoContextGetAttributes guest input](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Low.cpp) (libSceAudioOut) - copies each `AudioOut2Attribute` entry with `GuestCopyIn_nid_no_patch` while preserving host `errno`, returning `0x80268001` on unreadable entries instead of dereferencing guest memory directly; console behavior on unmapped memory is unmeasured.
