# libSceScreenShot technical debt

### Silent stubs

- [sceScreenShotEnableNotification and sceScreenShotDisableNotification](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - return 0 without effect: on the console they ask the system software to show or hide the capture notification, which this project does not display
- [sceScreenShotSetOverlayImage and sceScreenShotSetOverlayImageWithOrigin](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - check the arguments and the file as the PS5 module does, then return 0 without keeping the image, since this project takes no screenshots to overlay
- [sceScreenShotIsDisabled](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - reports one process-wide flag set by sceScreenShotDisable and cleared by sceScreenShotEnable for any id other than -1; on the console the system software answers, and what the id selects is unknown

### Unknown function info

- [sceScreenShotCapture, sceScreenShotGetAppInfo, sceScreenShotGetDrcParam, sceScreenShotIsVshScreenCaptureDisabled, sceScreenShotSetDrcParam and sceScreenShotSetParam](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceScreenShot.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified.
