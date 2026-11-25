# libSceAudioOut

## add Functional

- [sceAudioOut2PortSetAttributes guest inputs](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Port.cpp) (libSceAudioOut) - copies `AudioOut2Attribute` entries and data/volume payloads through `GuestCopyIn_nid_no_patch` while preserving host `errno`, returning `0x80260502` (`INVALID_ARGUMENT`) on unreadable pointers instead of dereferencing guest memory directly; console behavior on unmapped memory is unmeasured.
