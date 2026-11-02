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
- [sceRazorCpuFiberLogNameChange, sceRazorCpuFiberSwitch, sceRazorCpuGetDataTagStorageSize, sceRazorCpuGpuMarkerSync, sceRazorCpuInitializeGpuMarkerContext, sceRazorCpuSetPushMarkerCallback, sceRazorCpuSetPushMarkerStaticCallback, sceRazorCpuSetPopMarkerCallback, sceRazorCpuStartCaptureInternal, sceRazorCpuStopCaptureInternal, sceRazorCpuWorkloadSubmit, sceRazorCpuWorkloadRunBegin, sceRazorCpuWorkloadRunEnd](../../../core/libs/prx/libSceRazorCpu/Export.cpp) (libSceRazorCpu) - give the answers firmware 13.42's libSceRazorCpu.sprx gives while the profiler is not initialized, read from disassembly and not measured on a console, and record nothing: the three marker callbacks are discarded, start and stop capture always return 0x80580000, `sceRazorCpuGetDataTagStorageSize` returns 0, and the rest return 0 (`sceRazorCpuInitializeGpuMarkerContext` returns 22 for a null context or one whose first byte is not 1). Once initialized, the console sizes a tag table at 96 bytes per tag and calls the marker callbacks, and the GPU marker context is used; none of that happens here


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

- [sceRazorCpuInitializeInternal](../../../core/libs/prx/libSceRazorCpu/Export.cpp) (libSceRazorCpu) - throws: in firmware 13.42 it is the only call that sets the profiler's initialized flag, after it creates the capture storage, a mutex and a registered callback, and the capture paths that flag enables are not implemented. Its signature is unknown
- [sceRazorCpuJobManagerDispatch, sceRazorCpuJobManagerJob and sceRazorCpuJobManagerSequence](../../../core/libs/prx/libSceRazorCpu/Export.cpp): argument positions and serialized widths are recovered from machine instructions. Dispatch takes seven arguments, including two 64-bit payloads whose upper 16 bits are replaced by the count fields. The payloads are not dereferenced, so their original pointer-versus-integer types and the semantic names of the event identifiers remain unresolved. Job and Sequence serialize 16-bit identifiers and an event selector; their source enum names are unavailable.
- [sceRazorCpuFiberLogNameChange, sceRazorCpuFiberSwitch, sceRazorCpuGpuMarkerSync, sceRazorCpuStartCaptureInternal, sceRazorCpuStopCaptureInternal, sceRazorCpuWorkloadSubmit, sceRazorCpuWorkloadRunBegin, sceRazorCpuWorkloadRunEnd](../../../core/libs/prx/libSceRazorCpu/Export.cpp) (libSceRazorCpu) - do not read their arguments while the profiler is not initialized, so their signatures (including that of `sceRazorCpuGpuMarkerSync`) are unknown and they are declared without parameters
