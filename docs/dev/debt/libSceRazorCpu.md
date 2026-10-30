# libSceRazorCpu technical debt

### Silent stubs

- [libSceRazorCpu](../../../core/libs/prx/libSceRazorCpu/Export.cpp): capture-controller registration, thread-local capture storage, marker serialization and flushing are not implemented. The host implements only the confirmed uninitialized-profiler path: markers return zero without reading labels, and `sceRazorCpuFlushOccurred` returns false without writing its optional output.
- [libSceRazorCpu](../../../core/libs/prx/libSceRazorCpu/Export.cpp): the entry above covers only the markers and `sceRazorCpuFlushOccurred`. Fourteen further calls also return without reading their arguments or recording anything (`sceRazorCpuDisableFiberUserMarkers` returns nothing, the others zero). Three of them are imported by the 53 titles run here:

| Call | Titles importing it (of 53) |
| --- | --- |
| `sceRazorCpuSync` | 1 |
| `sceRazorCpuPlotValue` | 2 |
| `sceRazorCpuWriteBookmark` | 2 |

The other eleven are imported by none of them: `sceRazorCpuIsCapturing`, `sceRazorCpuNamedSync`, `sceRazorCpuDisableFiberUserMarkers`, `sceRazorCpuInitDataTags`, `sceRazorCpuShutdownDataTags`, `sceRazorCpuTagArray`, `sceRazorCpuTagBuffer`, `sceRazorCpuResizeTaggedBuffer`, `sceRazorCpuUnTagBuffer`, `sceRazorCpuBeginLogicalFileAccess` and `sceRazorCpuEndLogicalFileAccess`.

### Unknown function info

- [Additional profiling exports](../../../core/libs/prx/libSceRazorCpu/Export.cpp) - signatures and behavior are unknown. The parameterless `int` declarations only resolve imports, make no ABI claim and every call throws through `NotImplemented_nid_no_patch`. Names and NIDs are from the [13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscerazorcpu-98b3fdc87b.jsonl); not measured on a console.

| Name | NID |
| --- | --- |
| `sceRazorCpuUserSignalWaitBegin` | `7y2mIfBZO+I` |
| `sceRazorCpuUserSignalPost` | `9qdtVVdPT-0` |
| `sceRazorCpuWriteBookmarkEnd` | `Ay3X-hss71o` |
| `sceRazorCpuUserSignalWaitEnd` | `C0QNDdQW+8M` |
| `sceRazorCpuWorkloadRunBeginEx` | `Crha9LvwvJM` |
| `sceRazorCpuUserSignalPostEx` | `cKl+d8u8mbk` |
| `sceRazorCpuUserSignalWaitBeginEx` | `lC8-7cVnkHA` |

- [sceRazorCpuFiberLogNameChange, sceRazorCpuFiberSwitch, sceRazorCpuGpuMarkerSync, sceRazorCpuInitializeGpuMarkerContext, sceRazorCpuInitializeInternal, sceRazorCpuSetPopMarkerCallback](../../../core/libs/prx/libSceRazorCpu/Export.cpp) (libSceRazorCpu) - the list is the intersection of the export table of the console's [libSceRazorCpu](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceRazorCpu.c) (function symbols only), the pinned [aerolib names](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) and the functions not yet defined anywhere in the repo. No credible prototypes were found; they are declared without parameters solely to resolve known imports, not as an ABI claim, and callers must not call them until signatures are verified.
- [sceRazorCpuSetPushMarkerCallback, sceRazorCpuSetPushMarkerStaticCallback, sceRazorCpuStartCaptureInternal, sceRazorCpuStopCaptureInternal, sceRazorCpuWorkloadRunBegin, sceRazorCpuWorkloadRunEnd, sceRazorCpuWorkloadSubmit](../../../core/libs/prx/libSceRazorCpu/Export.cpp) (libSceRazorCpu) - the list is the intersection of the export table of the console's [libSceRazorCpu](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceRazorCpu.c) (function symbols only), the pinned [aerolib names](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) and the functions not yet defined anywhere in the repo. No credible prototypes were found; they are declared without parameters solely to resolve known imports, not as an ABI claim, and callers must not call them until signatures are verified.
- [sceRazorCpuJobManagerDispatch, sceRazorCpuJobManagerJob and sceRazorCpuJobManagerSequence](../../../core/libs/prx/libSceRazorCpu/Export.cpp): argument positions and serialized widths are recovered from machine instructions. Dispatch takes seven arguments, including two 64-bit payloads whose upper 16 bits are replaced by the count fields. The payloads are not dereferenced, so their original pointer-versus-integer types and the semantic names of the event identifiers remain unresolved. Job and Sequence serialize 16-bit identifiers and an event selector; their source enum names are unavailable.
- [sceRazorCpuGetDataTagStorageSize](../../../core/libs/prx/libSceRazorCpu/Export.cpp) (libSceRazorCpu) - throws: [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Debug/RazorCpu.cs#L142-L144) documents it as the byte size a tag table needs for a count of entries, but not the size of an entry
