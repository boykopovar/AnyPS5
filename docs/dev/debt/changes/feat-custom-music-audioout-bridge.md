# libSceCustomMusicAudioOut

## add Functional

- [libSceCustomMusicAudioOut audio output bridge](../../../core/libs/prx/libSceCustomMusicAudioOut/Export.cpp) (libSceCustomMusicAudioOut) - open and manage a `libSceAudioOut` port (`sceAudioOutOpen(0xff, 1, 0, a1, 48000, a2 == 1 ? a3 | 0x10000 : a3)`), delegate `sceCustomMusicAudioOutOutput` and `sceCustomMusicAudioOutGetLastOutputTime`, propagate port volumes (`sceAudioOutSetVolume`), and map `0x80260002`, `0x80260006`, `0x8026000D` to `0x81380004`, `0x81380001`, `0x81380002` (else `0x81380003`) following firmware 13.60 `libSceCustomMusicAudioOut.sprx` disassembly (`0x1ae0`, `0x1cf0`, `0x1f00`, `0x1fb0`, `0x2490`, `0x2890`, not measured on a console). Unwritable non-null `out` pointers return `0x81380001` as a host-safety fallback instead of faulting.
