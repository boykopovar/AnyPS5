# libSceComposite technical debt

### Silent stubs

- [sceCompositorCreateIndirectRenderTarget, sceCompositorDeleteIndirectRenderTarget, sceCompositorSetIndirectCanvasCommand, sceCompositorSetIndirectCompositionCommand and sceCompositorSetIndirectRenderTargetConfigCommand](../../../core/libs/prx/libSceComposite/Export.cpp) return error codes or success without managing compositor canvas or indirect render target state.

### Unknown function info

- [sceCompositorAllocateIndex, sceCompositorCheckCrash, sceCompositorCommandGpuPerfBegin, sceCompositorCommandGpuPerfEnd, sceCompositorFlush, sceCompositorFlushWithRepeat, sceCompositorGetAnotherProcessSystemAddress, sceCompositorGetAnotherProcessSystemSize, sceCompositorGetAnotherProcessVideoAddress, sceCompositorGetAnotherProcessVideoSize, sceCompositorGetCanvasHandle, sceCompositorGetClientList, sceCompositorGetDmemOffset, sceCompositorGetDummyAddress, sceCompositorGetDummySize, sceCompositorGetRemainedCommandNum, sceCompositorGetRenderTargetResolution, sceCompositorGetSystemAddress, sceCompositorGetSystemSize, sceCompositorGetVideoAddress, sceCompositorGetVideoSize, sceCompositorHandleProcessEvents, sceCompositorInit, sceCompositorInitWithProcessOrder, sceCompositorInsertThreadTraceMarker, sceCompositorIsDebugCaptureEnabled, sceCompositorIsReady4K, sceCompositorLockCommandBuffer, sceCompositorMakeCanvasHandle, sceCompositorMapAnotherProcess, sceCompositorMemoryPoolDecommit, sceCompositorPrivacyMaskPushArea, sceCompositorQuit, sceCompositorReleaseCommandBuffer, sceCompositorReleaseIndex, sceCompositorReleaseLocalStall, sceCompositorReserveCommandBuffer, sceCompositorResetZoomCommand, sceCompositorSetAgcCompositeCanvasCommand, sceCompositorSetAgcContextCommand, sceCompositorSetCompositeCanvasCommandInC, sceCompositorSetCursorImageAddress, sceCompositorSetDebugPositionCommand, sceCompositorSetEndOfFrameCommand, sceCompositorSetEventCommand, sceCompositorSetFilterType, sceCompositorSetFlipCommand2, sceCompositorSetGameSufaceControlCommand, sceCompositorSetGnmContextCommand, sceCompositorSetInvisibleCanvasCommand, sceCompositorSetKeepDisplayBufferCommand, sceCompositorSetLocalStallCommand, sceCompositorSetMemoryCommand, sceCompositorSetPatchCommand, sceCompositorSetPostEventCommand, sceCompositorSetRepeatCommand, sceCompositorSetResolutionCommand, sceCompositorSetScalingLayoutInfo, sceCompositorSetSeparateContextCommand, sceCompositorSetSystemConfigWithMask, sceCompositorSetVideoOutMode, sceCompositorSetVirtualCanvasAgcContextCommand, sceCompositorSetVirtualCanvasCommandFooter, sceCompositorSetVirtualCanvasPatchCommand, sceCompositorSetVrMode, sceCompositorSetZoomCommand, sceCompositorSystemConfig, sceCompositorWaitEndOfRendering, sceCompositorWaitEvent, sceCompositorWaitPostEvent, sceCompsoitorGetGpuClock, sceCompsoitorGetProcessRenderingTime and sceCompsoitorGetRenderingTime](../../../core/libs/prx/libSceComposite/Export.cpp) (libSceComposite) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceComposite.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceComposite/Export.cpp) - 45 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `sceCompositorGetMemoryPoolAddress` | `Kp5aInbKpf4` |
| `sceCompositorGetMemoryPoolSize` | `Vvd8eoo3OiM` |
| `sceCompositorGetNullCanvasHandle` | `+7a8xDlI+-s` |
| `sceCompositorGetSecondaryMemoryAddress` | `ReHQrFnGVxA` |
| `sceCompositorGetSecondaryMemorySize` | `5x5tC7zLwq4` |
| `sceCompositorGetTFRing` | `7pmp5rMZAHQ` |
| `sceCompositorGetVirtualCanvasHandle` | `Sd1bTXoZXQM` |
| `sceCompositorIsVirtualCanvasHandle` | `Bt2J+lHKAbQ` |
| `sceCompositorMemoryPoolCommit` | `-qZdNb3JOkg` |
| `sceCompositorMemoryPoolCreate` | `mQalCzqB93M` |
| `sceCompositorSecondaryMemoryCreate` | `TAlqzssvpes` |
| `sceCompositorSecondaryMemoryRelease` | `gEME-XiMfm0` |
| `sceCompositorSetCanvasIdToCanvasHandle` | `Y5h8Z7fhs-A` |
| `sceCompositorSetMetaIdToCanvasHandle` | `yDo24E2v2+Q` |
| `sceCompositorSetTFRing` | `kHYbbhOlSGM` |
| `sceCompositorSetVirtualCanvasRenderTargetCommand` | `MtUd+xuw74A` |
| `sceCompositorWaitEndOfRenderingIfGpuWorking` | `IUxijmT0S7g` |
| `Unknown` | `+A5zYfomrKo` |
| `Unknown` | `+DMKcAbpcrQ` |
| `Unknown` | `0hVj7ycuNjY` |
| `Unknown` | `3znAWbdvzNo` |
| `Unknown` | `4iL4YvcEKdU` |
| `Unknown` | `5vAisOgay3M` |
| `Unknown` | `8l316W539Zw` |
| `Unknown` | `FDHuP+z8KnM` |
| `Unknown` | `HXxq9giIX-I` |
| `Unknown` | `KpuS3CIcckw` |
| `Unknown` | `NMGq0MJ2iGo` |
| `Unknown` | `RPdjyM2WKcw` |
| `Unknown` | `SpfMcnnpfDA` |
| `Unknown` | `UEW01n5qkdA` |
| `Unknown` | `XZzyQX4T5ts` |
| `Unknown` | `Yck3L7ExyVc` |
| `Unknown` | `Z25ikyBUYy0` |
| `Unknown` | `aczOzib8mzI` |
| `Unknown` | `asXqPHp3O5g` |
| `Unknown` | `bwXAoMVv94M` |
| `Unknown` | `dTPMTtYqPX4` |
| `Unknown` | `gLAJaS1VbsA` |
| `Unknown` | `inplO6BVf5k` |
| `Unknown` | `j2CEWhcK0gc` |
| `Unknown` | `lvaCIMhYNh0` |
| `Unknown` | `mzPjqJ+unSw` |
| `Unknown` | `va+6TiqnE1Y` |
| `Unknown` | `yZcGEBwnDAM` |
