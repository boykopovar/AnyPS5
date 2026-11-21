# libSceCustomMusicAudioOut

## add Functional

- [sceCustomMusicAudioOut pointer safety](../../../core/libs/prx/libSceCustomMusicAudioOut/Export.cpp) (libSceCustomMusicAudioOut) - query and output functions `sceCustomMusicAudioOutGetVolume`, `sceCustomMusicAudioOutGetMuted`, `sceCustomMusicAudioOutGetAllMusicVolume`, and `sceCustomMusicAudioOutGetLastOutputTime` validate guest output pointer writability before checking initialization state, returning `0x81380001` for null or unwritable memory with caller `errno` preserved.
