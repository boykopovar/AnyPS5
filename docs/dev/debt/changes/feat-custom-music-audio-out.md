# libSceCustomMusicAudioOut

## remove sceCustomMusicAudioOutFinalize

## remove sceCustomMusicAudioOutGetLastOutputTime

## add Functional

- [sceCustomMusicAudioOutFinalize, sceCustomMusicAudioOutGetAllMusicVolume, sceCustomMusicAudioOutGetMuted, sceCustomMusicAudioOutGetVolume, sceCustomMusicAudioOutInitialize, sceCustomMusicAudioOutSetAllMusicVolume, sceCustomMusicAudioOutSetMuted and sceCustomMusicAudioOutSetVolume](../../../core/libs/prx/libSceCustomMusicAudioOut/Export.cpp) (libSceCustomMusicAudioOut) - volume and mute state tracking, clamping, level scaling, and lifecycle transitions are implemented according to firmware 13.42 disassembly (`libSceCustomMusicAudioOut.sprx` sha256 `15570e6d433612a291cda058d6179c7037af1e9f09fa6206c9c8838b1427b70e`). Guarded writes through `GuestCopyOut_nid_no_patch` return `0x81380001` preserving `errno` on inaccessible guest pointers.
