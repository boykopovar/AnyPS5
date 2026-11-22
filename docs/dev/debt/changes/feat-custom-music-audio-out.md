# libSceCustomMusicAudioOut

## remove sceCustomMusicAudioOutFinalize

## change sceCustomMusicAudioOutGetLastOutputTime

- [sceCustomMusicAudioOutGetLastOutputTime and sceCustomMusicAudioOutOutput](../../../core/libs/prx/libSceCustomMusicAudioOut/Export.cpp) (libSceCustomMusicAudioOut) - throw through `NotImplemented_nid_no_patch`. Names and NIDs are bound by the [13.60 firmware inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscecustommusicaudioout-b0940f5719.jsonl); argument validation and uninitialized checks are implemented from firmware 13.42 disassembly, but service audio output processing remains unimplemented.

## add Functional

- [sceCustomMusicAudioOutFinalize, sceCustomMusicAudioOutGetAllMusicVolume, sceCustomMusicAudioOutGetMuted, sceCustomMusicAudioOutGetVolume, sceCustomMusicAudioOutInitialize, sceCustomMusicAudioOutSetAllMusicVolume, sceCustomMusicAudioOutSetMuted and sceCustomMusicAudioOutSetVolume](../../../core/libs/prx/libSceCustomMusicAudioOut/Export.cpp) (libSceCustomMusicAudioOut) - volume and mute state tracking, clamping, level scaling, and lifecycle transitions are implemented according to firmware 13.42 disassembly (`libSceCustomMusicAudioOut.sprx` sha256 `15570e6d433612a291cda058d6179c7037af1e9f09fa6206c9c8838b1427b70e`). Guarded writes through `GuestCopyOut_nid_no_patch` return `0x81380001` preserving `errno` on inaccessible guest pointers.
