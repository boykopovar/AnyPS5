# libSceVisionManager technical debt

### Unknown function info

- [sceVisionManagerApplyCalibrateHmdResult, sceVisionManagerCancelRequest, sceVisionManagerFinalize, sceVisionManagerGenerateRegisterUserData, sceVisionManagerGenerateRegisterUserDataFinalize, sceVisionManagerGenerateRegisterUserDataInitialize, sceVisionManagerGetCalibrateTrackingLedInfo, sceVisionManagerGetCalibrateTrackingLedNecessity, sceVisionManagerGetHitCountOfRegisterUserData, sceVisionManagerGetLibraryVersion, sceVisionManagerGetNumOfRegisterUserDataSlot, sceVisionManagerGetRegisterUserDataSize, sceVisionManagerGetRegisterUserDataVersion, sceVisionManagerGetResultOfCalibrateHmd, sceVisionManagerGetResultOfCalibrateStereoCamera, sceVisionManagerGetResultOfCalibrateTrackingLed, sceVisionManagerGetResultOfFindFace, sceVisionManagerGetResultOfFindPad, sceVisionManagerGetResultOfFindPadCorrespondRegion, sceVisionManagerGetStoreCameraImgYPtr, sceVisionManagerGetWorkingMemorySize, sceVisionManagerInitialize, sceVisionManagerRegistryClear, sceVisionManagerRequestCalibrateHmd, sceVisionManagerRequestCalibrateStereoCamera, sceVisionManagerRequestCalibrateTrackingLed, sceVisionManagerRequestClose, sceVisionManagerRequestFindFace, sceVisionManagerRequestFindPad, sceVisionManagerRequestFindPadCorrespondRegion, sceVisionManagerRequestHeadDetection, sceVisionManagerRequestInitializeRegisterUserData, sceVisionManagerRequestOpen, sceVisionManagerRequestPadTracking, sceVisionManagerRequestRecogUtility, sceVisionManagerRequestStoreCameraImgY, sceVisionManagerSetCallbackForUpdateCalibrateHmdResult, sceVisionManagerSetCallbackForUpdateCalibrateStereoCameraResult, sceVisionManagerSetCallbackForUpdateCalibrateTrackingLedResult, sceVisionManagerSetCallbackForUpdateCameraFrame, sceVisionManagerSetCallbackForUpdateCtrlCore, sceVisionManagerSetCallbackForUpdateRecog, sceVisionManagerSetCallbackForUpdateRunningStatus, sceVisionManagerSetCallbackForUpdateStoreFrame and sceVisionManagerWaitResponse](../../../core/libs/prx/libSceVisionManager/Export.cpp) (libSceVisionManager) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVisionManager.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceVisionManager/Export.cpp) - 7 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `-R5dI6KjYpY` |
| `Unknown` | `0chNZ7GKUO0` |
| `Unknown` | `9gGQMrf6sWg` |
| `Unknown` | `ORAn12NcHYs` |
| `Unknown` | `YQiB-svtbcc` |
| `Unknown` | `dzN0VRFvA84` |
| `Unknown` | `wu4afKNlup4` |
