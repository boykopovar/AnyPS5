# libSceMsgDialog.native technical debt

### Silent stubs

- [libSceMsgDialog.native](../../../core/libs/prx/libSceMsgDialog.native/Export.cpp) - dialogs finish at `sceMsgDialogOpen` without UI and answer with button 1, so `sceMsgDialogClose` returns `NOT_RUNNING`; its result before `sceMsgDialogInitialize` follows libSceMsgDialog. `sceMsgDialogTerminate` resets running or finished dialogs and returns OK, rejecting uninitialized calls with `COMMON_DIALOG_ERROR_NOT_INITIALIZED` following shadPS4; null pointer arguments return `COMMON_DIALOG_ERROR_ARG_NULL`.
