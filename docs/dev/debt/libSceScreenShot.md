# libSceScreenShot technical debt

### Silent stubs

- [sceScreenShotEnableNotification and sceScreenShotDisableNotification](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - return 0 without effect: on the console they ask the system software to show or hide the capture notification, which this project does not display
- [sceScreenShotSetOverlayImage and sceScreenShotSetOverlayImageWithOrigin](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - check the arguments and the file as the PS5 module does, then return 0 without keeping the image, since this project takes no screenshots to overlay
- [sceScreenShotIsDisabled](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - reports one process-wide flag set by sceScreenShotDisable and cleared by sceScreenShotEnable for any id other than -1; on the console the system software answers, and what the id selects is unknown

### Unknown function info

- [sceScreenShotSetParam, Capture, GetAppInfo, IsVshScreenCaptureDisabled, GetDrcParam and SetDrcParam](../../../core/libs/prx/libSceScreenShot/Export.cpp) (libSceScreenShot) - argument lists and checks follow @BunnyFl's [firmware 13.42 run and disassembly](https://github.com/boykopovar/AnyPS5/issues/13737#issuecomment-6543610442), under an emulator with imports stubbed, not on a console; names are inferred. Each failed check returns 0x80BE0001 (the order is in the code). Past its checks each call would create a client for the screenshot service, which AnyPS5 doesn't have, so it throws. The module keeps no state between calls: `SetParam` sends its strings to the service instead of keeping them for `Capture`. `GetDrcParam`'s two counts (3rd and 6th arguments) are only used after the service call, and `SetDrcParam`'s 0x400 and 0x8000 fields look like counts of 4-byte elements (inferred). The service's answers, `Capture`'s cancel callback and the connect retries aren't modelled.
