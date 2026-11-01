# libSceScreenShot technical debt

### Silent stubs

- [sceScreenShotEnableNotification and sceScreenShotDisableNotification](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - return 0 without effect: on the console they ask the system software to show or hide the capture notification, which this project does not display
- [sceScreenShotSetOverlayImage and sceScreenShotSetOverlayImageWithOrigin](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - check the arguments and the file as the PS5 module does, then return 0 without keeping the image, since this project takes no screenshots to overlay
- [sceScreenShotIsDisabled](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - reports one process-wide flag set by sceScreenShotDisable and cleared by sceScreenShotEnable for any id other than -1; on the console the system software answers, and what the id selects is unknown

### Unknown function info

- [sceScreenShotGetDrcParam, sceScreenShotSetDrcParam and sceScreenShotIsVshScreenCaptureDisabled](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - member function signatures are recovered from the aerolib symbol database demangled names (`_ZN13sceScreenShot9IpcClient11SetDrcParamEjj`, `_ZN13sceScreenShot9IpcClient11GetDrcParamEiPjS1_` and `_ZN13sceScreenShot9IpcClient26IsVshScreenCaptureDisabledEPi`). The exported function prototypes are assumed from these internal `IpcClient` member names; this correspondence is unverified because members do not always match exports (for example, `SetOverlayImage` takes three integers in the client while the export takes a file path). Parameter semantics, valid identifiers, return values and console behavior remain unmeasured, so all three continue to throw `NotImplemented_nid_no_patch`.
- [sceScreenShotCapture, sceScreenShotGetAppInfo and sceScreenShotSetParam](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceScreenShot.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified.
