# libScePs2EmuMenuDialog technical debt

### Silent stubs

- [libScePs2EmuMenuDialog](../../../core/libs/prx/libScePs2EmuMenuDialog/Export.cpp) - the PS2 emulator menu dialog runs the common-dialog state machine without displaying host UI and reports the user as having cancelled (`COMMON_DIALOG_RESULT_USER_CANCELED = 1`). `scePs2EmuMenuDialogOpen` transitions directly to `FINISHED` (3) in headless mode, so `scePs2EmuMenuDialogClose` returns `COMMON_DIALOG_ERROR_NOT_RUNNING` (`0x80B8000B`). `scePs2EmuMenuDialogTerminate` matches the compare-and-swap form of sibling dialogs, but rejection of termination during active display remains unmeasured on a console.

### Unknown function info

- [scePs2EmuMenuDialogClose, scePs2EmuMenuDialogGetResult, scePs2EmuMenuDialogGetStatus, scePs2EmuMenuDialogInitialize, scePs2EmuMenuDialogOpen, scePs2EmuMenuDialogTerminate and scePs2EmuMenuDialogUpdateStatus](../../../core/libs/prx/libScePs2EmuMenuDialog/Export.cpp) (libScePs2EmuMenuDialog) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePs2EmuMenuDialog.c). Function signatures, parameter types and common-dialog error codes are inferred from sibling common dialogs rather than measured from console headers; the parameter structure for Open and result payload layout beyond the leading int32_t result code remain unverified.
