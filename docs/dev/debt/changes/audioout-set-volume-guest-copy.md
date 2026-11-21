# libSceAudioOut

## change [sceAudioOutSetVolume]

- [sceAudioOutSetVolume](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - safely copies `vol` via `GuestCopyIn_nid_no_patch` preserving caller `errno` and returns `0x80260004` on copy failure. Reads one volume per flagged channel from the array and stores it as given with no range check; that is an assumption, not measured on hardware. shadPS4 and fpPS4 read only the first value and return `0x80260009` when above 32768. KytyPS5 reads one per channel and checks no range, as here.
