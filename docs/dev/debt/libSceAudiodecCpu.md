# libSceAudiodecCpu technical debt

### Unknown function info

- [libSceAudiodecCpu codecs](../../../core/libs/prx/libSceAudiodecCpu/Export.cpp) - Argument checks and LPCM decode follow firmware 13.60 disassembly, unverified on console. Only 16- and 24-bit Blu-ray stereo LPCM decodes. These throw through `NotImplemented_nid_no_patch`: non-LPCM codecs (separate modules), DVD LPCM, non-stereo layouts (channel tables not recovered), frames larger than the 0x43800-byte input buffer, frames whose decoded PCM exceeds 0x43800 bytes (firmware 13.42 wraps output at 276,480 bytes, repeating earlier samples), an AU shorter than its frame or a PCM buffer smaller than the output (the module copies without checking), and `InitDecoder` without AU or PCM. Header byte 7 (frame count) and 33-bit timestamp continuation in the param are inferred. The type mask when `sceKernelIsCronos` returns non-zero is not modelled.
