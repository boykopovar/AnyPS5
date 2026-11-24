# libSceVisionManager technical debt

### Unknown function info

- [sceVisionManagerCancelRequest, sceVisionManagerFinalize, sceVisionManagerGenerateRegisterUserDataFinalize, sceVisionManagerGenerateRegisterUserDataInitialize, sceVisionManagerGetCalibrateTrackingLedInfo, sceVisionManagerGetRegisterUserDataSize, sceVisionManagerGetResultOfCalibrateHmd, sceVisionManagerGetResultOfCalibrateTrackingLed, sceVisionManagerGetStoreCameraImgYPtr, sceVisionManagerInitialize, sceVisionManagerRegistryClear, sceVisionManagerRequestCalibrateHmd, sceVisionManagerRequestCalibrateStereoCamera](../../../core/libs/prx/libSceVisionManager/Export.cpp) (libSceVisionManager) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVisionManager.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceVisionManagerRequestCalibrateTrackingLed, sceVisionManagerRequestClose, sceVisionManagerRequestFindFace, sceVisionManagerRequestHeadDetection, sceVisionManagerRequestRecogUtility, sceVisionManagerRequestStoreCameraImgY, sceVisionManagerSetCallbackForUpdateCameraFrame, sceVisionManagerSetCallbackForUpdateCtrlCore, sceVisionManagerSetCallbackForUpdateRecog, sceVisionManagerSetCallbackForUpdateStoreFrame and sceVisionManagerWaitResponse](../../../core/libs/prx/libSceVisionManager/Export.cpp) (libSceVisionManager) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVisionManager.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceVisionManagerGetLibraryVersion, sceVisionManagerGetNumOfRegisterUserDataSlot, sceVisionManagerGetWorkingMemorySize, sceVisionManagerRequestFindPad, sceVisionManagerRequestFindPadCorrespondRegion, sceVisionManagerRequestPadTracking, sceVisionManagerGetResultOfFindPad, sceVisionManagerGetResultOfFindPadCorrespondRegion and `-R5dI6KjYpY`](../../../core/libs/prx/libSceVisionManager/Export.cpp) - the parameter names and the layouts behind the pointers are unknown. They follow firmware 13.42's libSceVisionManager.sprx, read from its disassembly and run natively on x86-64 (not a console); argument types are inferred from register use. None of them reads library state. `sceVisionManagerGetLibraryVersion` writes 0x1020000214072421 and returns 0, or returns 0x80E70000 for a null pointer. `sceVisionManagerGetNumOfRegisterUserDataSlot(version, count)` writes 16 for version 0x1017000014043000 and 30 for 0x1020000214072421 and returns 0; any other version returns 0x80E70000 and writes nothing, and a null `count` returns 0x80E70000 for either version. `sceVisionManagerGetWorkingMemorySize` writes 0, 0x1600000 and 0x4200000 through its three pointers, without checking them, and returns 0. The pad requests never succeed: `sceVisionManagerRequestFindPad(context, parameter)` returns 0x80E70004 when both pointers are set and `*parameter` is nonzero and below 5 (signed), and 0x80E70000 otherwise; `sceVisionManagerRequestFindPadCorrespondRegion(context, parameter, region, result)` adds that `region` is 1 to 4 and `result` is set; `sceVisionManagerRequestPadTracking` ignores its first argument and uses a limit of 4. `sceVisionManagerGetResultOfFindPad` and `sceVisionManagerGetResultOfFindPadCorrespondRegion` return 0x80E70004 when all three pointers are set and 0x80E70000 otherwise. `-R5dI6KjYpY(input, output)` returns 0x80E70000 for a null pointer; otherwise it clears 32 bytes of `output`, writes 5 to `output[0]` when the larger of `input[0]` and `input[1]` (unsigned) is 10 to 29 and 10 when it is 30 or more, and returns 0
- [the null checks of 13 calls](../../../core/libs/prx/libSceVisionManager/Export.cpp) (libSceVisionManager) - only the null tests are modelled: each returns 0x80E70000 when one of the arguments in the table is null, and throws otherwise; what the calls do then is unknown. The parameter names and the layouts behind the pointers are unknown, and the types (`void*` for a tested argument, `std::uint64_t` for an untested one before it) are inferred from register use. Firmware 13.42's libSceVisionManager.sprx, read from its disassembly and run under a CPU emulator on x86-64 (not a console), tests these arguments first, before any call or state read; a null pointer is the only invalid value checked here.

| Export | Null-tested arguments |
| --- | --- |
| `sceVisionManagerApplyCalibrateHmdResult` | 1 |
| `sceVisionManagerGenerateRegisterUserData` | 1, 2 |
| `sceVisionManagerGetCalibrateTrackingLedNecessity` | 1, 2 |
| `sceVisionManagerGetHitCountOfRegisterUserData` | 1, 2, 4 |
| `sceVisionManagerGetRegisterUserDataVersion` | 1, 2, 3 |
| `sceVisionManagerGetResultOfCalibrateStereoCamera` | 1, 2 |
| `sceVisionManagerGetResultOfFindFace` | 1, 2, 3, 4 |
| `sceVisionManagerRequestInitializeRegisterUserData` | 1 |
| `sceVisionManagerRequestOpen` | 1 |
| `sceVisionManagerSetCallbackForUpdateCalibrateHmdResult` | 1 |
| `sceVisionManagerSetCallbackForUpdateCalibrateStereoCameraResult` | 1 |
| `sceVisionManagerSetCallbackForUpdateCalibrateTrackingLedResult` | 1 |
| `sceVisionManagerSetCallbackForUpdateRunningStatus` | 1, 2 |

### Functional

- [sceVisionManagerGetWorkingMemorySize](../../../core/libs/prx/libSceVisionManager/Export.cpp) (libSceVisionManager) - returns `0x80E70000` when output pointers are null or unwritable via `GuestCopyOut_nid_no_patch`, preserving host errno; console behavior on unmapped pointers is not measured.
- [sceVisionManagerRequestClose, sceVisionManagerRequestCalibrateHmd, sceVisionManagerRequestCalibrateStereoCamera, sceVisionManagerRequestCalibrateTrackingLed and sceVisionManagerRequestRecogUtility](../../../core/libs/prx/libSceVisionManager/Export.cpp) (libSceVisionManager) - they return `0x80E70025` and nothing else. In firmware 13.42 each first tests the library's open count and returns `0x80E70025` while it is 0; only `sceVisionManagerRequestOpen` raises it, and it still throws here, so the count stays 0. Once it is implemented, these must test the count and implement the open case. Read from the module's disassembly, not measured on a console.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceVisionManager/Export.cpp) - 7 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry except `-R5dI6KjYpY`, described under Unknown function info, throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `-R5dI6KjYpY` |
| `Unknown` | `0chNZ7GKUO0` |
| `Unknown` | `9gGQMrf6sWg` |
| `Unknown` | `ORAn12NcHYs` |
| `Unknown` | `YQiB-svtbcc` |
| `Unknown` | `dzN0VRFvA84` |
| `Unknown` | `wu4afKNlup4` |
