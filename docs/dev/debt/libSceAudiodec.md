# libSceAudiodec technical debt

### Unknown function info

- [M4AAC extended parameters](../../../core/libs/prx/libSceAudiodec/Export.cpp): 28-byte and 32-byte parameter variants add fields at offsets 24 and 28. Their accepted ranges are 0..1 and 0..2; nonzero modes throw until the corresponding decoder configuration is implemented. Zero extensions use the base decoding configuration.

- [sceAudiodecDecode](../../../core/libs/prx/libSceAudiodec/src/Codecs.cpp) (libSceAudiodec) - a raw M4AAC stream carries no channel configuration, so the decoder tries the mono/stereo configuration and then configurations 3 to 7 within `ui_max_channels` until one decodes the first access unit, and keeps that one. No reference documents how the library picks the layout (shadPS4 maps the channel count straight to the configuration; SharpProspero only describes the maximum). With the stereo configuration a mono raw stream decodes as 2 channels, so 2 is reported, not 1
- [sceAudiodecDecode](../../../core/libs/prx/libSceAudiodec/src/Codecs.cpp) (libSceAudiodec) - the HE-AAC handling follows one console measurement of an HE-AAC v2 ADTS stream: SBR output at 48 kHz in one channel with `ui_enable_heaac` 1, the 24 kHz mono core with 0. Not measured: whether the console, like FFmpeg, decodes a stream without SBR data in its first access unit as AAC-LC with `ui_enable_heaac` 1, and what it reports in `ui_sampling_freq` and `ui_heaac` with `ui_enable_heaac` 0 (here the rate of the returned PCM and 0)

### Functional

- [sceAudiodecDecode](../../../core/libs/prx/libSceAudiodec/src/Codecs.cpp) (libSceAudiodec) - an ADTS stream with channel configuration 0 (layout in a program config element) is still parsed by FFmpeg, so `ui_enable_heaac` 0 does not remove its SBR and a mono one with Parametric Stereo comes out as stereo.
- [libSceAudiodec](../../../core/libs/prx/libSceAudiodec/Export.cpp) accepts only 16-bit PCM, and float PCM for ATRAC9 and M4AAC, as `sceAudiodecCreateDecoder` did in a payload process on a PS5 (firmware 13.60), which returned `ERROR_INVALID_WORD_LENGTH` for `iBwPcm` 0, 3 and -1 with each codec and for float with MP3; other word sizes are assumed to fail the same way. ATRAC9 decoding is covered only by configuration and error tests, as no ATRAC9 encoder is available for a fixture.
