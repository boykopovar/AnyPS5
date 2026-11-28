# libSceAudiodecCpu

## change [libSceAudiodecCpu codecs]

- [libSceAudiodecCpu codecs](../../../core/libs/prx/libSceAudiodecCpu/Export.cpp) - Added 13.60-verified stereo DVD LPCM (`0x1004`, 16/20/24-bit, 150-tick PTS) decoding and HE-VAG (`0x1008`) `QueryMemSize` (`0x1b0`), `InitDecoder` validation/state setup, `ClearContext` history reset, and `Decode` pre-gates. Non-stereo LPCM layouts, DVD LPCM frames exceeding the `0x43800`-byte PCM output buffer, HE-VAG ADPCM sample decode, and external codec modules still throw through `NotImplemented_nid_no_patch`.
