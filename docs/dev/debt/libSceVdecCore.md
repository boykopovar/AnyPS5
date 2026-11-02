# libSceVdecCore technical debt

### Unknown function info
- [Additional libSceVdecCore exports](../../../core/libs/prx/libSceVdecCore/Export.cpp) - the name/NID pairs below are defined exports in the [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscevdeccore-219f557635.jsonl). Their ABIs and behaviour are unknown. Parameterless `int` declarations are import-resolution placeholders, not established signatures; every call throws `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `sceVdecCoreQueryMemorySizeSvp` | `uAsJCITzX5k` |


- [sceVdecCoreAnalyzeStream, sceVdecCoreFinalizeComputeResource, sceVdecCoreFlushDecodeOutput, sceVdecCoreGetDecodeOutput, sceVdecCoreInitializeComputeResource, sceVdecCoreResetDecoder, sceVdecCoreSetDecodeInput, sceVdecCoreSetDecodeOutputSw, sceVdecCoreSyncDecode, sceVdecCoreSyncDecodeOutputSw, sceVdecCoreSyncDecodeWptr, sceVdecCoreTrySyncDecode, sceVdecCoreTrySyncDecodeOutputSw and sceVdecCoreTrySyncDecodeWptr](../../../core/libs/prx/libSceVdecCore/Export.cpp) (libSceVdecCore) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecCore.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Functional

- [sceVdecCoreQueryInstanceSize, sceVdecCoreQueryFrameBufferInfo, sceVdecCoreQueryComputeResourceInfo, sceVdecCoreCreateDecoder, sceVdecCoreDeleteDecoder and sceVdecCoreMapMemoryBlock](../../../core/libs/prx/libSceVdecCore/Export.cpp) (libSceVdecCore) - console behaviour is unmeasured. The signatures, error codes, 1920x1080 and 16-frame defaults and slot cutting follow [shadPS4's `vdeccore.cpp`](https://github.com/shadps4-emu/shadPS4/blob/0614ce324be092791b758d79f93d9f1ca5b10a81/src/core/libraries/videodec/vdeccore.cpp#L275-L706), and the sizes are shadPS4's constants, not console values: instance 0x40000, compute 0x10000, and a frame buffer of NV12 plus 0x1000 rounded to 64 KiB. CreateDecoder opens an FFmpeg H.264 or HEVC context but starts no decode thread; the memory info and compute queue are ignored, and a handle CreateDecoder did not return, or a codec that is unavailable or fails to open, throws
