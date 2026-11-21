# libSceVorbisDec

## change [sceVorbisDecOpenCallbacks and sceVorbisDecBitrate]

- [sceVorbisDecOpenCallbacks](../../../core/libs/prx/libSceVorbisDec/Export.cpp) (libSceVorbisDec) - on Windows the host `long` is 32-bit: a tell result outside it, or an initial byte count outside it with a non-null `initial`, throws, and a sample rate of 2^31 Hz or more is rejected as a bad header.
