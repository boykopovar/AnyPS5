# libSceUsbStorageDialog technical debt

### Silent stubs

- [libSceUsbStorageDialog](../../../core/libs/prx/libSceUsbStorageDialog/Export.cpp) (libSceUsbStorageDialog) - the USB storage dialog runs the common-dialog state machine without showing a UI and reports the dialog as canceled by the user (`COMMON_DIALOG_RESULT_USER_CANCELED = 1`). `sceUsbStorageDialogOpen` transitions to `RUNNING` (2), while `sceUsbStorageDialogUpdateStatus` and `sceUsbStorageDialogClose` transition to `FINISHED` (3) before returning results, matching other common dialogs in the repository.

### Unknown function info

- [libSceUsbStorageDialog](../../../core/libs/prx/libSceUsbStorageDialog/Export.cpp) - the service-specific result status at offset 12 is not produced because the UI always cancels. The 80-byte result header and SDK-dependent validation are recovered; the remaining 64 bytes are untouched by the native wrapper. The purpose of the Open parameter size field remains unverified; the native wrapper does not validate it.
