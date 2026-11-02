# libmdbg_syscore technical debt

### Silent stubs

- [sceDebugGetProcessEventCntlFlag, sceDebugSetProcessEventCntlFlag](../../../core/libs/prx/libmdbg_syscore/Export.cpp) (libmdbg_syscore) - return 1 silently without inspecting or updating any debug event control flag state

### Unknown function info

- [Additional platform-debug exports](../../../core/libs/prx/libmdbg_syscore/Export.cpp) - the five name/NID pairs below are defined exports in the [13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libmdbg_syscore-53c596f5b7.jsonl). Their signatures and behavior are unverified. Parameterless `int` declarations are import placeholders, not verified ABI prototypes; every call throws through `NotImplemented_nid_no_patch`. No debug-counter, shell event or result-delivery behavior is implemented.

| Name | NID |
| --- | --- |
| `sceDebugGetAmprCounterInfo` | `ObMAmE6qMSw` |
| `sceMdbgFinalizeForShellCore` | `txc1CarzXK8` |
| `sceMdbgInitForShellCore` | `DFWZL-CN90E` |
| `sceMdbgReceiveEvent` | `zgg+ApEodYA` |
| `sceMdbgSetResult` | `TyelACu501E` |

- [sceDebugAcquireAndUpdateDebugRegister, sceDebugAttachProcess, sceDebugCancelCoredump, sceDebugClearStepThread, sceDebugCreatePerfScratchDataArea, sceDebugCreatePerfScratchExecutableArea, sceDebugCreateScratchDataArea, sceDebugCreateScratchDataAreaForPrx, sceDebugCreateScratchExecutableArea, sceDebugCreateScratchExecutableAreaForPrx, sceDebugDestroyPerfScratchDataArea, sceDebugDestroyPerfScratchExecutableArea, sceDebugDestroyScratchDataArea, sceDebugDestroyScratchExecutableArea, sceDebugDetachProcess, sceDebugGetApplicationIdByTitleId, sceDebugGetApplicationInfo, sceDebugGetApplicationList, sceDebugGetCrashInfoDetailForCoredump, sceDebugGetCrashInfoForCoredump, sceDebugGetDLLoadFlag, sceDebugGetDataListOfUltQueue, sceDebugGetDebugRegisterStatusMap, sceDebugGetEventList, sceDebugGetEventListForEQueueFd, sceDebugGetEventSubscriptionList, sceDebugGetEventSubscriptionListForEQueueFd, sceDebugGetFiberInfo, sceDebugGetFileInfo, sceDebugGetFileInfoForCoredump, sceDebugGetFileList, sceDebugGetFileListForCoredump, sceDebugGetGpuInfoArea, sceDebugGetJobManagerInfo, sceDebugGetJobManagerSequenceInfo, sceDebugGetJobManagerSequenceList, sceDebugGetModuleInfo, sceDebugGetModuleList, sceDebugGetModuleMetaData, sceDebugGetMonoVMInfo, sceDebugGetMonoVMList, sceDebugGetProcessCoredumpHandlerEventBlob, sceDebugGetProcessCoredumpHandlerEventInfo, sceDebugGetProcessCoredumpHandlerResult, sceDebugGetProcessInfo, sceDebugGetProcessList, sceDebugGetProcessPropertyForCoredump, sceDebugGetProcessResourceStatCount, sceDebugGetProcessResourceStatData, sceDebugGetProcessTime, sceDebugGetProcessTimeCounter, sceDebugGetSyncExclusiveWaiterList, sceDebugGetSyncObjectData, sceDebugGetSyncObjectList, sceDebugGetSyncWaiterList, sceDebugGetSyncWaiterListForEQueueFd, sceDebugGetSystemStatusBlob, sceDebugGetSystemStatusCount, sceDebugGetThreadInfo, sceDebugGetThreadInfoByIdent, sceDebugGetThreadList, sceDebugGetThreadListAsIdent, sceDebugGetUlObjectList, sceDebugGetUltCondvarInfo, sceDebugGetUltInfo, sceDebugGetUltListOfUltRuntime, sceDebugGetUltMutexInfo, sceDebugGetUltQueueDataResourcePoolInfo, sceDebugGetUltQueueInfo, sceDebugGetUltRuntimeInfo, sceDebugGetUltRwlockInfo, sceDebugGetUltSemaphoreInfo, sceDebugGetUltWaitingQueueResourcePoolInfo, sceDebugGetVirtualMemoryDetailInfoForCoredump, sceDebugGetVirtualMemoryInfo, sceDebugGetVirtualMemoryInfoForCoredump, sceDebugGetWaitingListOfUltCondvar, sceDebugGetWaitingListOfUltMutex, sceDebugGetWaitingListOfUltQueue, sceDebugGetWaitingListOfUltRwlock, sceDebugGetWaitingListOfUltSemaphore, sceDebugGetWorkerThreadListOfUltRuntime, sceDebugInit, sceDebugInitForCoredump, sceDebugInitForTest, sceDebugIpmiGetBlockTimeInfoList, sceDebugIpmiGetBlockedIpcInfo, sceDebugIpmiGetChannelInfo, sceDebugIpmiGetChannelKidList, sceDebugIpmiGetChannelWaitingThreadList, sceDebugIpmiGetClientInfo, sceDebugIpmiGetClientKidList, sceDebugIpmiGetClientKidListByDump, sceDebugIpmiGetClientKidListByServerKid, sceDebugIpmiGetConnectRequestInfoList, sceDebugIpmiGetConnectionInfoList, sceDebugIpmiGetConnectionWaitingThreadListByClientKid, sceDebugIpmiGetConnectionWaitingThreadListBySessionKid, sceDebugIpmiGetDump, sceDebugIpmiGetEncryptedInfoAllForCoredump, sceDebugIpmiGetKidInfoListForCoredump, sceDebugIpmiGetServerDispatchInfo, sceDebugIpmiGetServerInfo, sceDebugIpmiGetServerKidList, sceDebugIpmiGetServerKidListByDump, sceDebugIpmiGetServerWaitingThreadList, sceDebugIpmiGetSessionInfo, sceDebugIpmiGetSessionKidList, sceDebugIpmiGetSessionKidListByServerKid, sceDebugIpmiGetTidListByDump, sceDebugKillApplication, sceDebugKillProcess, sceDebugNoStopChildProcesses, sceDebugNoStopOnDLLoad, sceDebugProcessSpawn, sceDebugReadEvent, sceDebugReadEventForTest, sceDebugReadProcessMemory, sceDebugReadProcessMemoryForSDBGP, sceDebugReadProcessRegister, sceDebugReadProcessRegisterForSDBGP, sceDebugReadProcessResourceStatData, sceDebugReadThreadRegister, sceDebugReadThreadRegisterForSDBGP, sceDebugReleaseDebugRegister, sceDebugResumeApplication, sceDebugResumeProcess, sceDebugResumeThread, sceDebugSetStepThread, sceDebugSpawnApplication, sceDebugStopChildProcesses, sceDebugStopOnDLLoad, sceDebugSuspendApplication, sceDebugSuspendProcess, sceDebugSuspendThread, sceDebugTriggerCoredump, sceDebugTriggerCoredumpForSystem, sceDebugTriggerVrCaptureDump, sceDebugWriteProcessMemory, sceDebugWriteProcessRegister and sceDebugWriteThreadRegister](../../../core/libs/prx/libmdbg_syscore/Export.cpp) (libmdbg_syscore) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libmdbg_syscore.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libmdbg_syscore/Export.cpp) - 23 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `-Os5MER7Z6w` |
| `Unknown` | `63yQzeKlH74` |
| `Unknown` | `9Z4GhOWZL6U` |
| `Unknown` | `AOE76xS3KE8` |
| `Unknown` | `HAuOlaAs6n0` |
| `Unknown` | `KSF5BlhhUwA` |
| `Unknown` | `KnjWLw4OOEg` |
| `Unknown` | `MOLL-OygpCA` |
| `Unknown` | `Wlgt1fK0WIg` |
| `Unknown` | `Y4XKdJ9yi9w` |
| `Unknown` | `g0xdmqI4Kfo` |
| `Unknown` | `gRIqTUqqQmo` |
| `Unknown` | `h3hpY6kIB3c` |
| `Unknown` | `hAZNCht1+T0` |
| `Unknown` | `iJTmUN3+2A8` |
| `Unknown` | `iLMtKDbkQwM` |
| `Unknown` | `irUK9RMoEQo` |
| `Unknown` | `mORgO8Woq+E` |
| `Unknown` | `oNkb+v3gFg0` |
| `Unknown` | `oSQ+lkHQhI8` |
| `Unknown` | `og6sdu-o9GE` |
| `Unknown` | `qi6vA+rh1K4` |
| `Unknown` | `wrKerXOBJ8U` |
