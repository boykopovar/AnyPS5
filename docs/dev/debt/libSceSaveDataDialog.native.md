# libSceSaveDataDialog.native technical debt

### Silent stubs

- [libSceSaveDataDialog.native](../../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp) shows no UI and ignores progress values. Progress-bar mode (5) stays running until the caller closes it, matching Skyrim PPSA03747's save sequence; other modes still finish immediately and return button 1 and the first nonempty directory name. Console dialog timing and selection behavior remain unverified. Lifecycle state error codes follow shadPS4: `sceSaveDataDialogClose` returns `SAVE_DATA_DIALOG_ERROR_NOT_RUNNING` whenever not running, `sceSaveDataDialogGetResult` returns `SAVE_DATA_DIALOG_ERROR_NOT_FINISHED` whenever not finished (checked before null pointer), and `sceSaveDataDialogTerminate` closes running dialogs and returns OK, rejecting uninitialized calls with `SAVE_DATA_DIALOG_ERROR_NOT_INITIALIZED`, and progress updates (`sceSaveDataDialogProgressBarInc`, `sceSaveDataDialogProgressBarSetValue`) return `SAVE_DATA_DIALOG_ERROR_NOT_RUNNING` whenever not running.
  Parameter size in `sceSaveDataDialogOpen` is unvalidated as console behaviour is unmeasured.
- [sceSaveDataDialogIsReadyToDisplay](../../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp) returns 1 without checking dialog initialization or display readiness.

### Unknown function info

- [sceSaveDataDialogOpen](../../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp) (libSceSaveDataDialog.native) - no overlay dialog is drawn for any mode: the layouts of the user message, system message and error code parameters that the `SaveDataDialogParam` pointers lead to have no source in this repository or measurement on a PS5, so a dialog is not built from them. Closes when: the parameter layouts are measured on a PS5
