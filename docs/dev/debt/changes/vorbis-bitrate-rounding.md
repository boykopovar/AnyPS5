# libSceVorbisDec

## change [sceVorbisDecOpenCallbacks and sceVorbisDecBitrate]

- [sceVorbisDecOpenCallbacks](../../../core/libs/prx/libSceVorbisDec/Export.cpp) (libSceVorbisDec) - on Windows an initial byte count outside the host's 32-bit `long` range throws when `initial` is non-null, and a sample rate of 2^31 Hz or more is rejected as a bad header.
