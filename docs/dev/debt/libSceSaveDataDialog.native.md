# libSceSaveDataDialog.native technical debt

### Silent stubs

- [libSceSaveDataDialog.native](../../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp) shows no UI and ignores progress values. Progress-bar mode stays running until closed, matching Skyrim PPSA03747; other modes finish immediately returning button 1 and the first directory name. Console timing and selection remain unverified. State error codes follow shadPS4: `sceSaveDataDialogClose` returns `SAVE_DATA_DIALOG_ERROR_NOT_RUNNING` when not running, `sceSaveDataDialogGetResult` returns `SAVE_DATA_DIALOG_ERROR_NOT_FINISHED` before null checks, `sceSaveDataDialogTerminate` closes running dialogs, and progress updates (`sceSaveDataDialogProgressBarInc`, `sceSaveDataDialogProgressBarSetValue`) return `SAVE_DATA_DIALOG_ERROR_NOT_RUNNING` when not running, validating target 0 and rate bounds up to 100 with `SAVE_DATA_DIALOG_ERROR_PARAM`. Parameter size in `sceSaveDataDialogOpen` is unvalidated as console behaviour is unmeasured.
- [sceSaveDataDialogIsReadyToDisplay](../../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp) returns 1 unconditionally without checking state, following shadPS4 (savedatadialog.cpp at ce74386283); native console readiness state is unmeasured.

### Unknown function info

- [sceSaveDataDialogOpen](../../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp) (libSceSaveDataDialog.native) - no overlay dialog is drawn for any mode: the layouts of the user message, system message and error code parameters that the `SaveDataDialogParam` pointers lead to have no source in this repository or measurement on a PS5, so a dialog is not built from them. Closes when: the parameter layouts are measured on a PS5
