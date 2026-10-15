# libSceUsbStorageDialog technical debt

### Silent stubs

- [libSceUsbStorageDialog](../../../core/libs/prx/libSceUsbStorageDialog/Export.cpp) - the USB storage dialog runs the common-dialog state machine without showing a UI and reports the dialog as canceled by the user (`COMMON_DIALOG_RESULT_USER_CANCELED = 1`). Open and GetResult take a pointer, while Initialize, GetStatus, UpdateStatus, Close and Terminate take none, matching the standard PS5 common dialog lifecycle.
