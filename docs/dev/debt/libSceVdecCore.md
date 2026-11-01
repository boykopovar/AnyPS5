# libSceVdecCore technical debt

### Unknown function info
- [Additional libSceVdecCore exports](../../../core/libs/prx/libSceVdecCore/Export.cpp) - the name/NID pairs below are defined exports in the [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscevdeccore-219f557635.jsonl). Their ABIs and behaviour are unknown. Parameterless `int` declarations are import-resolution placeholders, not established signatures; every call throws `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `sceVdecCoreQueryMemorySizeSvp` | `uAsJCITzX5k` |


- [sceVdecCoreAnalyzeStream, sceVdecCoreCreateDecoder, sceVdecCoreDeleteDecoder, sceVdecCoreFinalizeComputeResource, sceVdecCoreFlushDecodeOutput, sceVdecCoreGetDecodeOutput, sceVdecCoreInitializeComputeResource, sceVdecCoreMapMemoryBlock, sceVdecCoreQueryComputeResourceInfo, sceVdecCoreQueryFrameBufferInfo, sceVdecCoreQueryInstanceSize, sceVdecCoreResetDecoder, sceVdecCoreSetDecodeInput, sceVdecCoreSetDecodeOutputSw, sceVdecCoreSyncDecode, sceVdecCoreSyncDecodeOutputSw, sceVdecCoreSyncDecodeWptr, sceVdecCoreTrySyncDecode, sceVdecCoreTrySyncDecodeOutputSw and sceVdecCoreTrySyncDecodeWptr](../../../core/libs/prx/libSceVdecCore/Export.cpp) (libSceVdecCore) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecCore.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
