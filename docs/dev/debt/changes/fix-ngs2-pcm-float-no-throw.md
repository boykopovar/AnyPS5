# libSceNgs2.native

## change NGS2 PCM sampler playback
- [NGS2 PCM sampler playback](../../../core/libs/prx/libSceNgs2.native/src/Pcm.cpp) is not console-verified on hardware. Types 0x11, 0x12, 0x14, 0x16, 0x18 and 0x1a follow [prosper's waveform parser analysis](https://github.com/mattias800/prosper/blob/30572df43ae20ff28f995db56149f7af42235741/prosper/src/hle/audio/ngs2_waveform.cpp#L194-L223) and 13.60 `libSceNgs2.native.sprx` (`0x8530`-`0x8c7b`): floating `f32`/`f64` samples (`0x18`/`0x1a`) are read and narrowed via `vcvtsd2ss` without range, nonfinite or subnormal exceptions. Nonzero config words and other types still throw, and non-`i16` formats require complete blocks.
